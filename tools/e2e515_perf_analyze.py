"""Read-only game/sys-clk log analysis. Never changes clocks or SD configuration."""
from __future__ import annotations
import argparse, bisect, csv, datetime as dt, hashlib, json, math, re
from collections import defaultdict
from pathlib import Path

def number(value):
    try:
        v=float(value)
        return v if math.isfinite(v) else None
    except (ValueError,TypeError):return None

def timestamp(value,utc_offset=None):
    n=number(value)
    if n is not None:
        if n>=1e17:return n/1e6
        if n>=1e14:return n/1000
        if n>=1e11:return n
        if n>=1e9:return n*1000
        return None # Not an epoch; never join guessed uptime to calendar time.
    try:
        t=dt.datetime.fromisoformat(value.replace('Z','+00:00'))
        if t.tzinfo is None:
            if utc_offset is None:return None
            t=t.replace(tzinfo=dt.timezone(dt.timedelta(minutes=utc_offset)))
        return t.timestamp()*1000
    except (ValueError,TypeError,AttributeError):return None

def title(value):
    s=str(value).lower().removeprefix('0x')
    return s.zfill(16) if re.fullmatch('[0-9a-f]{1,16}',s) and int(s,16) else ''

def read_csv(path):
    with path.open(encoding='utf-8-sig',errors='replace',newline='') as f:
        preview=f.read(8192);f.seek(0)
        delimiter=';' if preview.splitlines() and ';' in preview.splitlines()[0] else ','
        return list(csv.DictReader((line for line in f if not line.startswith('#')),delimiter=delimiter))

def sysclk(path,offset=None):
    rows=read_csv(path);result=[];bad=0
    for row in rows:
        raw={str(k).strip():v for k,v in row.items() if k is not None}
        data={k.lower():v for k,v in raw.items()}
        epoch=timestamp(data.get('timestamp',data.get('time')),offset)
        if epoch is None:bad+=1;continue
        normalized={'epoch_ms':epoch,'profile':data.get('profile',''),
                    'app_tid':title(data.get('app_tid',data.get('title_id',''))),'raw':raw}
        for name,value in data.items():
            n=number(value)
            if name.endswith('_hz') and n is not None:
                normalized[name[:-3]+'_mhz']=n/1e6 if n>0 else None
            elif name.endswith('_millic') and n is not None:
                normalized[name[:-7]+'_c']=n/1000
            elif name.endswith('_mw') and n is not None:normalized[name]=n
            elif name.endswith(('_mhz','_c')):normalized[name]=n
        result.append(normalized)
    return sorted(result,key=lambda r:r['epoch_ms']),bad

def percentile(values,fraction):
    return sorted(values)[math.ceil((len(values)-1)*fraction)] if values else None

def analyze(game,clock=None,stage_log=None,clock_log=None,title_id='',max_align_ms=1500,utc_offset=None,screens=None):
    raw=read_csv(game);rows=[];bad=0
    for r in raw:
        try:
            rows.append({**r,**{k:int(r[k]) for k in ['mono_us','epoch_ms','wall_us','cpu_us','screen','a','b']}})
        except (ValueError,TypeError,KeyError):bad+=1
    if not rows:raise ValueError('No valid game samples')
    rows.sort(key=lambda r:r['mono_us'])
    anchors=[r for r in rows if r['stage']=='anchor' and r['epoch_ms']>0 and r['a']==0]
    anchor_times=[r['mono_us'] for r in anchors]
    warnings=[]
    if not anchors:warnings.append('No calendar anchors; sys-clk correlation unavailable')
    offsets=[r['epoch_ms']-r['mono_us']/1000 for r in anchors]
    clock_jump=bool(offsets and max(offsets)-min(offsets)>100)
    if clock_jump:warnings.append('Calendar clock changed >100 ms; cross-clock alignment disabled')
    stage_text=stage_log.read_text(encoding='utf-8',errors='replace') if stage_log else ''
    ids=set(title(s) for s in re.findall(r'program_id=([0-9a-fA-F]+)',stage_text));ids.discard('')
    wanted=title(title_id) if title_id else next(iter(ids)) if len(ids)==1 else ''
    if not wanted and clock:warnings.append('Title ID unknown/ambiguous; no cross-application join attempted')
    clocks,clock_bad=sysclk(clock,utc_offset) if clock else ([],0)
    external_fields=sorted(set().union(*(set(r)-{'raw'} for r in clocks))) if clocks else []
    if clocks and not any(k.endswith('_mhz') for k in external_fields):
        warnings.append('Frequency columns/units unrecognized; inspect raw telemetry without guessing units')
    compatible=[r for r in clocks if wanted and r['app_tid']==wanted]
    times=[r['epoch_ms'] for r in compatible]
    if clock and not compatible:warnings.append('No sys-clk rows with the matching title ID')
    battle_screens=set(screens) if screens is not None else {r['screen'] for r in rows if r['stage']=='timer' and r['screen']>=0}
    frames=[r for r in rows if r['stage']=='frame' and (not battle_screens or r['screen'] in battle_screens)]
    aligned=[]
    for r in frames:
        epoch=None
        if anchors and not clock_jump:
            at=max(0,bisect.bisect_right(anchor_times,r['mono_us'])-1)
            anchor=anchors[at];epoch=anchor['epoch_ms']+(r['mono_us']-anchor['mono_us'])/1000
        match=None;delta=None
        if epoch is not None and compatible:
            i=bisect.bisect_left(times,epoch)
            choices=[j for j in (i-1,i) if 0<=j<len(times)]
            j=min(choices,key=lambda j:abs(times[j]-epoch));delta=times[j]-epoch
            if abs(delta)<=max_align_ms:match=compatible[j]
        aligned.append(dict(mono_us=r['mono_us'],epoch_ms=epoch,screen=r['screen'],frame_ms=r['wall_us']/1000,
            logical_ticks=r['a'],max_gap_ms=r['b']/1000,clock_delta_ms=delta if match else None,
            **({k:v for k,v in match.items() if k not in {'raw','epoch_ms'}} if match else {})))
    stats={};grouped=defaultdict(list)
    for r in rows:
        if screens is not None and r['screen'] not in battle_screens:continue
        if r['stage'] in {'anchor','session','logical_tick','memory','backlog','heartbeat','frame'}:continue
        grouped[r['stage']].append(r)
    for stage,samples in grouped.items():
        walls=[r['wall_us']/1000 for r in samples];cpus=[r['cpu_us']/1000 for r in samples if r['cpu_us']>=0]
        stats[stage]=dict(count=len(samples),wall_total_ms=sum(walls),wall_avg_ms=sum(walls)/len(walls),
            wall_p50_ms=percentile(walls,.5),wall_p95_ms=percentile(walls,.95),wall_p99_ms=percentile(walls,.99),
            wall_max_ms=max(walls),cpu_total_ms=sum(cpus) if cpus else None,cpu_samples=len(cpus))
    drops=[int(s) for s in re.findall(r'dropped=(\d+)',stage_text)]
    if drops and max(drops):warnings.append(f'Queue samples lost: cumulative dropped={max(drops)}')
    if re.search(r'(?:perf_failed|log_failed)=1|capped=1|capped=\d/1',stage_text):warnings.append('Writer I/O failure or size cap; data incomplete')
    if '# file_cap_reached' in game.read_text(encoding='utf-8',errors='replace') or '# file_cap_reached' in stage_text:
        warnings.append('File size limit reached; later samples unavailable')
    if bad or clock_bad:warnings.append(f'Invalid rows: game={bad}, sys-clk={clock_bad}')
    if not frames:warnings.append('No completed frame samples; hardware recording not established')
    gaps=[r['wall_us']/1000 for r in frames if r['wall_us']>0]
    health=[r for r in rows if r['stage']=='heartbeat']
    tick_health=[r for r in health if r['screen'] in battle_screens]
    tick_window=(tick_health[-1]['mono_us']-tick_health[0]['mono_us'])/1e6 if len(tick_health)>1 else 0
    counter_ticks=tick_health[-1]['b']-tick_health[0]['b'] if tick_window else None
    summary=dict(status='ANALYZED DATA ONLY; hardware provenance supplied by caller',
        build_markers=sorted(set(re.findall(r'build_git=([^\s]+)',stage_text))),
        modes=sorted(set(r['a'] for r in rows if r['stage']=='session')),
        applet_types=sorted(set(int(s) for s in re.findall(r'applet_type=(-?\d+)',stage_text))),
        title_id=wanted or None,game_samples=len(rows),frame_samples=len(frames),battle_screens=sorted(battle_screens),
        explicit_screen_filter=sorted(screens) if screens is not None else None,
        tick_counter_window_s=tick_window or None,ticks_from_cumulative_heartbeat=counter_ticks,
        actual_ticks_per_second=counter_ticks/tick_window if tick_window and counter_ticks>=0 else None,
        tick_rate_scope='Completed ArcadeTickCount increments, heartbeat interval; includes paused/result periods, not a per-active-mode rate',
        fps_from_intervals=1000/(sum(gaps)/len(gaps)) if gaps else None,
        frame_p50_ms=percentile(gaps,.5),frame_p95_ms=percentile(gaps,.95),frame_max_ms=max(gaps) if gaps else None,
        max_observed_no_frame_ms=max((r['wall_us']/1000 for r in health),default=0),
        sysclk_rows=len(clocks),matched_frames=sum(r['clock_delta_ms'] is not None for r in aligned),
        external_fields=external_fields,
        anchor_max_bracket_us=max((r['wall_us'] for r in anchors),default=None),
        time_alignment='DISABLED clock jump' if clock_jump else 'epoch anchors; nearest matching title, bounded tolerance',
        warnings=warnings,stages=stats,
        causal_status={key:'UNDETERMINED; measurements and controlled comparison required' for key in
            ['CPU limit','GPU limit','tick scheduling','excess redraw','memory management','clock or thermal throttling']})
    if clock_log:
        text=clock_log.read_text(encoding='utf-8',errors='replace')
        summary['sysclk_log_evidence']=[line for line in text.splitlines() if re.search(r'sys.?clk|hoc.?clk|version|freq|temp|power|error',line,re.I)][:200]
    summary['input_sha256']={str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in [game,clock,stage_log,clock_log] if p}
    return summary,aligned,rows,clocks

def plots(out,rows,clocks):
    try:
        import matplotlib
        matplotlib.use('Agg')
        import matplotlib.pyplot as plt
    except ImportError:return 'matplotlib unavailable; numeric outputs preserved'
    start=min(r['mono_us'] for r in rows)
    fig,axes=plt.subplots(5,1,figsize=(13,13),constrained_layout=True,sharex=True)
    for label in ['frame','timer','advance_total','draw','present']:
        selected=[r for r in rows if r['stage']==label]
        if selected:axes[0].plot([(r['mono_us']-start)/1e6 for r in selected],[r['wall_us']/1000 for r in selected],label=label,linewidth=.8,marker='.',markersize=3)
    axes[0].set(ylabel='Wall time (ms)',xlabel='Monotonic elapsed (s)');axes[0].legend()
    for label in ['backlog','memory']:
        selected=[r for r in rows if r['stage']==label and r['a']>=0]
        ax=axes[1] if label=='backlog' else axes[2]
        if selected:ax.plot([(r['mono_us']-start)/1e6 for r in selected],[r['a'] if label=='backlog' else r['a']/1048576 for r in selected],label=label,marker='.',markersize=3)
        ax.set(ylabel='Backlog (ms)' if label=='backlog' else 'Process RAM (MiB)',xlabel='Monotonic elapsed (s)')
    if clocks:
        anchor=next((r for r in rows if r['stage']=='anchor' and r['epoch_ms']>0 and r['a']==0),None)
        epoch_start=anchor['epoch_ms']+(start-anchor['mono_us'])/1000 if anchor else clocks[0]['epoch_ms']
        keys=set().union(*(r.keys() for r in clocks))
        for key in sorted(keys):
            if key.endswith('_mhz') or key.endswith('_c'):
                selected=[r for r in clocks if isinstance(r.get(key),(float,int))]
                if selected:axes[3 if key.endswith('_mhz') else 4].plot([(r['epoch_ms']-epoch_start)/1000 for r in selected],[r[key] for r in selected],label=key,linewidth=.8,marker='.',markersize=3)
    axes[3].set(ylabel='Clock (MHz)',xlabel='Elapsed from game origin (s)')
    axes[4].set(ylabel='Temperature (C)',xlabel='Elapsed from game origin (s)')
    for ax in axes:
        ax.grid(alpha=.3)
        if not ax.lines:ax.text(.5,.5,'No available samples',ha='center',va='center',transform=ax.transAxes)
        if ax.lines and ax is not axes[0]:ax.legend(fontsize=8)
    fig.suptitle('Recorded samples; gaps, dropped data and inclusive scopes require review')
    fig.savefig(out/'performance.png',dpi=150);plt.close(fig)
    return 'performance.png; game time origin, clocks mapped through epoch anchor'

def main(argv=None):
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--game',type=Path,required=True);p.add_argument('--stage-log',type=Path)
    p.add_argument('--sys-clk-csv',type=Path);p.add_argument('--sys-clk-log',type=Path)
    p.add_argument('--title-id',default='');p.add_argument('--max-align-ms',type=float,default=1500)
    p.add_argument('--csv-utc-offset-minutes',type=int);p.add_argument('--out',type=Path,required=True)
    p.add_argument('--screen',type=int,action='append',help='Restrict frames/stage statistics to actual screen ID; repeat to select several')
    args=p.parse_args(argv)
    if args.max_align_ms<=0:p.error('max-align-ms must be positive')
    summary,aligned,rows,clocks=analyze(args.game,args.sys_clk_csv,args.stage_log,args.sys_clk_log,args.title_id,args.max_align_ms,args.csv_utc_offset_minutes,args.screen)
    args.out.mkdir(parents=True,exist_ok=False)
    fields=sorted(set().union(*(r.keys() for r in aligned))) if aligned else ['mono_us','frame_ms']
    with (args.out/'aligned.csv').open('w',encoding='utf-8',newline='') as f:
        w=csv.DictWriter(f,fieldnames=fields);w.writeheader();w.writerows(aligned)
    (args.out/'sysclk-normalized.json').write_text(json.dumps(clocks,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    plot_clocks=[r for r in clocks if r['app_tid']==summary['title_id']] if not summary['time_alignment'].startswith('DISABLED') else []
    plot_rows=[r for r in rows if r['screen'] in args.screen] if args.screen is not None else rows
    summary['plot_status']=plots(args.out,plot_rows,plot_clocks) if plot_rows else 'No selected screen samples'
    (args.out/'summary.json').write_text(json.dumps(summary,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    report=['# Performance analysis','',summary['status'],'',
        'Frame samples: '+str(summary['frame_samples'])+'; matched sys-clk frames: '+str(summary['matched_frames']),
        '','Warnings: '+('; '.join(summary['warnings']) or 'none detected; not a hardware PASS'),
        '','| Inclusive stage | Count | Avg wall ms | p95 wall ms | Max wall ms | CPU samples |',
        '|---|---:|---:|---:|---:|---:|']
    for stage,s in sorted(summary['stages'].items(),key=lambda pair:pair[1]['wall_total_ms'],reverse=True):
        report.append(f"| {stage} | {s['count']} | {s['wall_avg_ms']:.3f} | {s['wall_p95_ms']:.3f} | {s['wall_max_ms']:.3f} | {s['cpu_samples']} |")
    report+=['','Nested stages overlap. Never sum parent and child timings.',
        'CPU samples use own-thread execution time where available; wall time includes waits.',
        'Clock frequency alone does not measure GPU utilization or establish throttling.',
        '','![Recorded performance](performance.png)' if summary['plot_status'].startswith('performance.png') else summary['plot_status'],'']
    report += [f'- {k}: {v}' for k,v in summary['causal_status'].items()]
    (args.out/'report.md').write_text('\n'.join(report)+'\n',encoding='utf-8')
    print(json.dumps({k:v for k,v in summary.items() if k not in ['stages','input_sha256','sysclk_log_evidence']},ensure_ascii=False,indent=2))
    return 0
if __name__=='__main__':raise SystemExit(main())
