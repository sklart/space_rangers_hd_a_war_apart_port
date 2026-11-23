"""Show resource use of the isolated Windows autosave reference process."""
import psutil
import sys

name = sys.argv[1].lower() if len(sys.argv) > 1 else 'gov-frun-autosave'
for process in psutil.process_iter(['pid', 'name', 'cpu_times', 'memory_info', 'status']):
    if name in (process.info['name'] or '').lower():
        print(process.info)
