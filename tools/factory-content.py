"""Fetch pinned build inputs from existing PUBLIC downloads; no GitHub account needed.

Only writes into this source tree, never into installed applications or profiles.
Factory data keeps its separate licences; retrieval is not a relicensing grant.
"""
import argparse,hashlib,json,os,sys,tempfile,urllib.request,zipfile
from pathlib import Path,PurePosixPath
ROOT=Path(__file__).resolve().parents[1]
def sha(p):
 with p.open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
def local(name):
 parts=PurePosixPath(name).parts
 if not parts or any(p in ('..','.') for p in parts) or '\\' in name or ':' in name or name.startswith('/'):raise ValueError('Unsafe input path')
 p=ROOT.joinpath(*parts)
 if not p.resolve().is_relative_to(ROOT.resolve()) or any(a.is_symlink() for a in [p,*p.parents] if a!=ROOT.parent):raise ValueError('Linked or external input path')
 return p
def matches(p,item):return p.is_file() and p.stat().st_size==item['bytes'] and sha(p)==item['sha256']
def fetch(url,expected,cache,size=None,archive=None):
 if archive:
  p=Path(archive).resolve()
  if sha(p)!=expected:raise ValueError('Provided archive checksum mismatch')
  return p
 cache.mkdir(parents=True,exist_ok=True);p=cache/expected
 if p.is_file() and sha(p)==expected:return p
 if not url.startswith('https://'):raise ValueError('HTTPS source required')
 with tempfile.NamedTemporaryFile(dir=cache,delete=False) as temp:
  tmp=Path(temp.name)
  try:
   count=0
   with urllib.request.urlopen(url,timeout=60) as response:
    while chunk:=response.read(1024*1024):
     count+=len(chunk)
     if count>(size if size is not None else 80*1024*1024):raise ValueError('Download exceeds pinned size limit')
     temp.write(chunk)
   temp.close()
   if size is not None and count!=size:raise ValueError('Download size mismatch')
   if sha(tmp)!=expected:raise ValueError('Downloaded checksum mismatch')
   os.replace(tmp,p)
  finally:
   temp.close()
   if tmp.exists():tmp.unlink()
 return p
def content(args):
 lock=json.loads((ROOT/'Resources/public-content-lock.json').read_text(encoding='utf-8'))
 pending=[i for i in lock['files'] if not matches(local(i['path']),i)]
 if args.command=='verify':
  if pending:raise ValueError(str(len(pending))+' content files missing/different; run sync first')
  print('Public content inventory verified:',len(lock['files']),'files');return
 if not pending:print('Content already verified');return
 for item in pending:
  if item['source']=='repository':raise ValueError('Restore reviewed source file: '+item['path'])
  if local(item['path']).exists():raise ValueError('Preserve local edit; use a clean source tree: '+item['path'])
 bundle=lock['bundle'];archive=fetch(bundle['url'],bundle['sha256'],ROOT/'.public-downloads',bundle['bytes'],args.archive)
 with zipfile.ZipFile(archive) as z:
  if len(z.namelist())!=len(set(z.namelist())):raise ValueError('Duplicate archive members')
  for item in pending:
   member=z.getinfo(item['member'])
   if member.file_size!=item['bytes'] or (member.external_attr>>16)&0o170000==0o120000:raise ValueError('Unexpected bundled member')
   data=z.read(member)
   if hashlib.sha256(data).hexdigest()!=item['sha256']:raise ValueError('Bundled member checksum mismatch')
   target=local(item['path']);target.parent.mkdir(parents=True,exist_ok=True)
   with target.open('xb') as stream:stream.write(data)
 print('Installed verified PUBLIC bundle data:',len(pending),'files')
def sdk(args):
 spec=json.loads((ROOT/'dependency-downloads.json').read_text(encoding='utf-8'))['webview2']
 destination=local(spec['destination']);receipt=destination/'PUBLIC-SDK-SHA256.txt'
 if receipt.exists() and receipt.read_text().strip()==spec['sha256']:print('Pinned SDK present');return
 archive=fetch(spec['url'],spec['sha256'],ROOT/'.public-downloads',archive=args.archive)
 if destination.exists() and any(destination.iterdir()):raise ValueError('SDK destination is not empty')
 with zipfile.ZipFile(archive) as z:
  for info in z.infolist():
   if info.is_dir():continue
   parts=PurePosixPath(info.filename).parts
   if any(x in ('..','.') for x in parts) or '\\' in info.filename or ':' in info.filename or info.filename.startswith('/'):raise ValueError('Unsafe SDK member')
   if (info.external_attr>>16)&0o170000==0o120000:raise ValueError('Linked SDK member')
   p=local(spec['destination']+'/'+info.filename);p.parent.mkdir(parents=True,exist_ok=True)
   with p.open('xb') as stream:stream.write(z.read(info))
 destination.mkdir(parents=True,exist_ok=True);receipt.write_text(spec['sha256']+'\n',encoding='utf-8');print('Pinned public Microsoft SDK installed')
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('command',choices=['sync','verify','sdk']);p.add_argument('--archive',type=Path);a=p.parse_args()
 if a.command=='sdk':sdk(a)
 else:content(a)
if __name__=='__main__':
 try:main()
 except (ValueError,OSError,KeyError,zipfile.BadZipFile) as error:raise SystemExit('Public inputs: '+str(error))
