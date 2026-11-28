from pathlib import Path
import subprocess,sys,os,json
R=Path(__file__).resolve().parent;O=R/'vm-control-evidence';O.mkdir(exist_ok=True)
results=[]
def run(cmd,label,env=None):
 p=subprocess.run(cmd,cwd=R,env=env,capture_output=True,text=True,timeout=180)
 (O/(label+'.log')).write_text(p.stdout+p.stderr);results.append(dict(label=label,exit=p.returncode))
 print(label,p.returncode,flush=True)
 p.check_returncode()
for delay in [0,30]:
 run([sys.executable,'tools/test_ppu_prio_start.py','--negative-controls','--host-delay-ms',str(delay)],'contracts-negative-delay-'+str(delay))
# Compile once, then run a predetermined sample; any failure fails the gate.
link=['-Wl,-dead_strip'] if sys.platform=='darwin' else ['-Wl,--gc-sections','-pthread','-lm']
b=O/'integration';subprocess.run(['cc','-std=gnu17','-D_GNU_SOURCE','-O1','-UNDEBUG','-ffunction-sections','-fdata-sections','-Iinclude','-Iruntime','-Iruntime/syscalls','tests/test_ppu_prio_start.c',*link,'-o',str(b)],cwd=R,check=True)
for i in range(200):
 env=dict(os.environ);env.pop('PS3_PRIO_START',None)
 if i%2:env['PS3_PRIO_START']='1'
 run([str(b)],f'sample-{i:03d}',env)
(O/'results.json').write_text(json.dumps(results,indent=2))
print('PASS: 200 fixed-sample integrations and 22 correctness negative controls; no retries')
