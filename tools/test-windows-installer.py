"""Exercise only the separate Review AppId and a marked workspace sandbox."""
from pathlib import Path
import hashlib,json,os,shutil,struct,subprocess,time,winreg
import argparse
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--installer',type=Path,required=True);parser.add_argument('--fixture',type=Path,required=True);parser.add_argument('--work',type=Path,required=True)
args=parser.parse_args();O=Path(__file__).resolve().parents[1];W=args.work.resolve();W.mkdir(parents=True,exist_ok=True);E=W/'evidence';E.mkdir(exist_ok=True);B=W/'installer-sandbox';APP=B/'Program';PROFILE=B/'Roaming/ToneGrid';DOCS=B/'Documents/ToneGrid/Recordings'
assert B.resolve().is_relative_to(W.resolve()) and not B.exists()
B.mkdir();(B/'.tonegrid-installer-test').write_text('ToneGrid isolated installer test; no production data\n')
for p in (PROFILE/'Rigs-v1',PROFILE/'Audio',PROFILE/'Preferences',DOCS):p.mkdir(parents=True,exist_ok=True)
for name,data in {'Rigs-v1/slot-001.tonexrig':b'<PedalRig version="3" name="Isolated user sentinel"/>','Audio/settings.xml':b'<AudioSettings keep="true"/>','Preferences/language.json':b'{"locale":"fr"}'}.items():(PROFILE/name).write_bytes(data)
(DOCS/'personal-take.wav').write_bytes(b'Isolated synthetic recording-preservation sentinel; not real audio\n')
def h(p):
 with p.open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
def profile_hashes():return {p.relative_to(B).as_posix():h(p) for r in (PROFILE,DOCS) for p in r.rglob('*') if p.is_file()}
original_profile=profile_hashes();results=[]
def check(value,label):
 results.append(dict(check=label,passed=bool(value)))
 (E/'installer-tests.json').write_text(json.dumps(dict(results=results,allPassed=all(x['passed'] for x in results)),indent=2)+'\n',encoding='utf-8')
 if not value:raise AssertionError(label)
def run(name,args,timeout=300):
 start=time.time();print('START',name,flush=True)
 with (E/(name+'.console.log')).open('w',encoding='utf-8') as log:
  proc=subprocess.Popen([str(x) for x in args],env=env,stdout=log,stderr=subprocess.STDOUT,creationflags=subprocess.CREATE_NO_WINDOW)
  try:code=proc.wait(timeout=timeout)
  except subprocess.TimeoutExpired:
   proc.terminate();proc.wait(timeout=20);raise RuntimeError('Stopped timed-out isolated test process: '+name)
 check(code==0,name+' exits successfully')
 print('DONE',name,'seconds',round(time.time()-start,2),flush=True)
env=dict(os.environ);temp=B/'Temp';temp.mkdir();env.update(TEMP=str(temp),TMP=str(temp))
reg=r'Software\Microsoft\Windows\CurrentVersion\Uninstall\ToneGrid.Windows.LocalReview.20261026_is1'
def registration():
 try:
  with winreg.OpenKey(winreg.HKEY_CURRENT_USER,reg,0,winreg.KEY_READ|winreg.KEY_WOW64_64KEY) as key:
   return {n:winreg.QueryValueEx(key,n)[0] for n in ('InstallLocation','DisplayName','Publisher','UninstallString')}
 except FileNotFoundError:return None
check(registration() is None,'No previous Review installation is reused')
fixture=args.fixture.resolve();final=args.installer.resolve()
base=['/VERYSILENT','/SUPPRESSMSGBOXES','/NORESTART','/NOCLOSEAPPLICATIONS','/TASKS=desktopicon','/TESTROOT='+str(B)]
run('install-fixture',[fixture,*base,'/DIR='+str(APP),'/LOG='+str(E/'install-fixture.log')])
info=registration();check(info is not None and Path(info['InstallLocation']).resolve()==APP.resolve(),'Per-user Review registration uses the isolated program folder')
check(info['Publisher']=='L’équipe ToneGrid','Public publisher identity is L’équipe ToneGrid')
for p in (B/'Shortcuts/Programs/ToneGrid Review.lnk',B/'Shortcuts/Desktop/ToneGrid Review.lnk'):check(p.is_file(),'Created shortcut '+p.parent.name)
check(profile_hashes()==original_profile,'Initial install preserves profile and Documents byte-for-byte')
# Test upgrade pruning with both an unchanged owned file and a local modification.
check((APP/'InstallerUpgradeFixture/retired-stock.txt').is_file(),'Fixture contains known previous owned stock file')
modified=APP/'InstallerUpgradeFixture/retired-modified.txt';modified.write_text('Keep this local modification\n',encoding='utf-8')
user_added=APP/'user-added.txt';user_added.write_text('Untracked user-added file\n',encoding='utf-8')
run('upgrade-to-final',[final,*base,'/LOG='+str(E/'upgrade-to-final.log')])
check(Path(registration()['InstallLocation']).resolve()==APP.resolve(),'Upgrade reuses previous directory without a DIR override')
check(not (APP/'InstallerUpgradeFixture/retired-stock.txt').exists(),'Upgrade retires only unchanged obsolete owned file')
check(modified.read_text(encoding='utf-8')=='Keep this local modification\n','Upgrade preserves modified obsolete program file')
check(user_added.exists(),'Upgrade preserves an untracked user-added file')
check(profile_hashes()==original_profile,'Upgrade preserves user presets, settings, language and recording byte-for-byte')
manifest=json.loads((APP/'PACKAGE-MANIFEST.json').read_bytes())
check(all((APP/n).is_file() and h(APP/n)==v['sha256'] for n,v in manifest['files'].items()),'All installed runtime members match the verified package SHA256 inventory')
def unsigned(p):
 with p.open('rb') as f:
  f.seek(0x3c);off=struct.unpack('<I',f.read(4))[0];f.seek(off+24);magic=struct.unpack('<H',f.read(2))[0]
  f.seek(off+24+(112 if magic==0x20b else 96)+32);return struct.unpack('<II',f.read(8))==(0,0)
uninstaller=APP/'unins000.exe'
for p in (APP/'ToneGrid.exe',final,uninstaller):check(unsigned(p),'No Authenticode certificate in '+p.name)
# These modes explicitly provide an isolated application-data root and synthetic audio.
for flag,label in (('--startup143-ui-test','installed-startup-fresh'),('--startup143-existing-ui-test','installed-startup-existing')):
 run(label,[APP/'ToneGrid.exe',flag],timeout=240)
 source=APP/'Startup143Tests'/('fresh' if label.endswith('fresh') else 'existing')
 target=E/label;shutil.copytree(source,target,dirs_exist_ok=True)
 report=(target/'startup143-tests.txt').read_text(encoding='utf-8-sig')
 check('FAIL ' not in report and 'renders finite audio' in report,label+' reports resources, saved tone, UI readiness and finite audio')
check(profile_hashes()==original_profile,'Isolated application tests preserve installer-test user data')
run('uninstall-review',[uninstaller,'/VERYSILENT','/SUPPRESSMSGBOXES','/NORESTART','/LOG='+str(E/'uninstall-review.log')])
for _ in range(20):
 if not (APP/'ToneGrid.exe').exists() and registration() is None:break
 time.sleep(.5)
check(not (APP/'ToneGrid.exe').exists(),'Uninstall removes the owned application executable')
check(registration() is None,'Uninstall removes only Review registration')
check(not any((B/'Shortcuts').rglob('*.lnk')),'Uninstall removes owned shortcuts')
check(profile_hashes()==original_profile,'Uninstall retains all isolated user profile and recording bytes')
check(user_added.exists(),'Uninstall leaves an untracked user-added file')
print('Installer lifecycle complete:',len(results),'checks.',flush=True)
