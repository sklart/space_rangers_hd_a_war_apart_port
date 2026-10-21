"""Query-local memoization of repeated collision-line endpoint projections."""
def apply(text):
    if text.startswith('#include "e2e516_stop_projection.hpp"'):
        if text.count('endpoint_memo.Get(')!=2:raise RuntimeError('Incomplete stop projection memo')
        return text
    start=text.index('    void ab_StopLine_GetDistances(')
    end=text.index('\n    }',start)+len('\n    }')
    part=text[start:end]
    marker='            Line = FirstCollisionLine;'
    if part.count(marker)!=1:raise RuntimeError('Stop-line loop changed')
    part=part.replace(marker,'''            auto project_endpoint = [&](const TabStopPoint* point) {
                return ab_Global::ProjectPointByMatrix(Matrix, pas::ConstRef<EC_Struct::TVector3D>(&point->Position));
            };
            srhd_awa::platform::StopProjectionMemo<TabStopPoint,EC_Struct::TVector3D,decltype(project_endpoint)> endpoint_memo(project_endpoint);
'''+marker,1)
    for field in ('First','Last'):
        old=f'ab_Global::ProjectPointByMatrix(Matrix, pas::ConstRef<EC_Struct::TVector3D>(&Line->{field}->Position))'
        if part.count(old)!=1:raise RuntimeError('Stop-line endpoint changed: '+field)
        part=part.replace(old,f'endpoint_memo.Get(Line->{field})',1)
    return '#include "e2e516_stop_projection.hpp"\n'+text[:start]+part+text[end:]

def reference(text):
    """Remove only this optimization for controlled diagnostic A/B builds."""
    if not text.startswith('#include "e2e516_stop_projection.hpp"'):return text
    text=text.removeprefix('#include "e2e516_stop_projection.hpp"\n')
    start=text.index('            auto project_endpoint = ')
    end=text.index('            Line = FirstCollisionLine;',start)
    text=text[:start]+text[end:]
    for field in ('First','Last'):
        text=text.replace(f'endpoint_memo.Get(Line->{field})',f'ab_Global::ProjectPointByMatrix(Matrix, pas::ConstRef<EC_Struct::TVector3D>(&Line->{field}->Position))')
    return text
