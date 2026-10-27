#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
from pathlib import Path
import os,subprocess,tempfile,time,statistics,json
import pexpect,ctypes as C
from PIL import Image
import argparse,shutil,signal
from x11 import Desktop as Display,wait_for
ap=argparse.ArgumentParser(description="Test xterm in an isolated Helios image and Xvfb")
for name in ('image','template','ioserver','binary','probe','defaults','output'):
    ap.add_argument('--'+name,type=Path,required=True)
ap.add_argument('--font-check',type=Path,help='native font comparison; defaults beside the built terminal-check')
ap.add_argument('--t4',default='t4');ap.add_argument('--xvfb',default='Xvfb')
ap.add_argument('--cpu',choices=('s4','s8'),default='s8')
ap.add_argument('--display',type=int,default=84)
a=ap.parse_args()
for name in ('image','template','ioserver','binary','probe','defaults','output'):
    setattr(a,name,getattr(a,name).resolve())
a.font_check=(a.font_check or a.probe.parent/'font-check').resolve()
out=a.output;out.mkdir(parents=True,exist_ok=True)
assert not Path('/tmp/.X11-unix/X'+str(a.display)).exists(),'Test display is already in use'
folder=Path(tempfile.mkdtemp(prefix='xterm412-',dir=a.image/'tmp'))
for source,name in ((a.binary,'xterm412'),(a.probe,'terminal-check'),(a.defaults,'XTerm412'),(a.font_check,'font-check')):
    shutil.copy2(source,folder/name)
native='/helios/tmp/'+folder.name
log=folder/'terminal.log'
tmp=tempfile.TemporaryDirectory(prefix='xterm412-run-');sock=Path(tmp.name)/'link.sock'
settings={'helios_directory':str(a.image),'system_image':str(a.image/'lib/nucleus'),
          'bootfile':str(a.image/'lib/nboot.i'),'t4_link':'unix:'+str(sock),
          'target_processor':'T800' if a.cpu=='s8' else 'T425',
          'logfile':str(out/'ioserver.log'),'logging_destination':'file'}
lines=[]
for line in a.template.read_text().splitlines():
    key=line.split('=')[0].strip().lower()
    if key not in settings:lines.append(line)
lines += [key+' = '+value for key,value in settings.items()]
assert any(line.split('=')[-1].strip().lower()=='t4' for line in lines if line.split('=')[0].strip()=='box'),'Use an emulator configuration template'
con=Path(tmp.name)/'host.con';con.write_text('\n'.join(lines)+'\n')
def stop(process):
    if process is not None and process.poll() is None:
        process.terminate()
        try:process.wait(timeout=3)
        except subprocess.TimeoutExpired:process.kill();process.wait()
xlog=open(out/'xvfb.log','w');xvfb=subprocess.Popen([a.xvfb,':'+str(a.display),'-screen','0','1000x700x24','-listen','tcp','-ac'],stdout=xlog,stderr=xlog)
srv=t4=None
try:
 for _ in range(50):
  if Path('/tmp/.X11-unix/X'+str(a.display)).exists():break
  time.sleep(.1)
 d=Display(':'+str(a.display))
 tlog=open(out/'t4.log','w');t4=subprocess.Popen([a.t4,'-'+a.cpu,'-sm','24','-se','-sh','unix:'+str(sock)],stdout=tlog,stderr=tlog,start_new_session=True)
 srv=pexpect.spawn(str(a.ioserver),['-C',str(con)],env=dict(os.environ,TERM='xterm'),encoding='latin1',timeout=30,dimensions=(40,120));trace=open(out/'console.log','w');srv.logfile_read=trace
 srv.expect('login:');srv.sendline('root');srv.expect('% ')
 srv.sendline(native+'/font-check 127.0.0.1:'+str(a.display));srv.expect('PASS font ownership',timeout=120);srv.expect('% ')
 print('PASS exact font metrics, widths, font errors and ownership',flush=True)
 srv.sendline('setenv XENVIRONMENT '+native+'/XTerm412');srv.expect('% ')
 srv.sendline(native+'/xterm412 -display 127.0.0.1:'+str(a.display)+' -geometry 80x25+20+20 -title Xterm412-check -xrm "*cursorBlink: false" +sb &');srv.expect('% ')
 for _ in range(100):
  w=d.find('Xterm412-check')
  if w:break
  time.sleep(.1)
 print('window',w,flush=True)
 time.sleep(1)
 d.screenshot(str(out/'first.png'))
 if not w:raise AssertionError('No window')
 wait_for(lambda:d.attrs(w).map_state==2)
 d.focus(w)
 # Measure actual key-to-render time. Empty Xt polls used to add several
 # 10 ms Helios select waits to each character, even on an idle display.
 d.type('echo ');time.sleep(.3);latency=[]
 for key in 'abcdefghijkl':
  before=d.image(400,120,w).tobytes();start=time.monotonic();d.key(key)
  while d.image(400,120,w).tobytes()==before:
   assert time.monotonic()-start<2,'No visible keyboard echo'
   time.sleep(.002)
  latency.append((time.monotonic()-start)*1000);time.sleep(.15)
 (out/'latency.json').write_text(json.dumps(latency,indent=2)+'\n')
 assert statistics.median(latency[1:])<50,latency
 print('PASS keyboard echo: median %.1f ms'%statistics.median(latency[1:]),flush=True)
 d.key('Return');time.sleep(.3)
 d.type(native+'/terminal-check '+native+'/terminal.log\n')
 deadline=time.monotonic()+30
 while time.monotonic()<deadline:
  text=log.read_text() if log.exists() else ''
  # Finish every cursor reply before resizing: SIGWINCH can interrupt Read.
  if 'latin1-restored:' in text:break
  time.sleep(.1)
 print('queries:',text,flush=True)
 assert 'utf8:' in text
 d.resize(w,904,634);time.sleep(.5)
 d.type('jJ');d.key('Escape');d.key('1');d.key('Escape');d.key('2');d.key('Up')
 ctrl=d.x.XKeysymToKeycode(d.d,d.x.XStringToKeysym(b'Control_L'))
 d.xt.XTestFakeKeyEvent(d.d,ctrl,1,0);d.key('d');d.xt.XTestFakeKeyEvent(d.d,ctrl,0,0);d.x.XSync(d.d,0)
 deadline=time.monotonic()+10
 while time.monotonic()<deadline:
  text=log.read_text()
  if 'keys:' in text:break
  time.sleep(.1)
 print('result:',text,flush=True)
 d.screenshot(str(out/'terminal-check.png'))
 assert 'resize=100x35' in text,text
 assert 'keys: 6a 4a 1b 31 1b 32 9b 41 04' in text,text
 for line in text.splitlines():
  if line.startswith(('osc-st:','unknown-osc:','osc-bel:')):assert line.endswith('1b 5b 31 3b 31 52'),line
  if line.startswith('utf8-c1-bytes:'):assert line.endswith('1b 5b 31 3b 33 52'),line
  if line.startswith('native-csi-utf8:'):assert line.endswith('1b 5b 31 3b 33 52'),line
  if line.startswith('utf8:'):assert line.endswith('1b 5b 31 3b 35 52'),line
  if line.startswith(('latin1-default:','latin1-restored:')):assert line.endswith('1b 5b 31 3b 33 52'),line
 # Check the real middle-button paste action and bracketed-paste wrappers.
 paste=folder/'paste.log'
 d.type(native+'/terminal-check '+native+'/paste.log --paste\n')
 wait_for(lambda:paste.exists() and 'paste-ready' in paste.read_text())
 d.x.XStoreBytes.argtypes=[C.c_void_p,C.c_char_p,C.c_int]
 d.x.XStoreBytes(d.d,b'PASTE123',8);d.x.XSync(d.d,0)
 d.motion(300,300);d.xt.XTestFakeButtonEvent(d.d,2,1,0);d.xt.XTestFakeButtonEvent(d.d,2,0,0);d.x.XSync(d.d,0)
 time.sleep(.3)
 d.xt.XTestFakeKeyEvent(d.d,ctrl,1,0);d.key('d');d.xt.XTestFakeKeyEvent(d.d,ctrl,0,0);d.x.XSync(d.d,0)
 wait_for(lambda:'keys:' in paste.read_text())
 expected='keys: 1b 5b 32 30 30 7e 50 41 53 54 45 31 32 33 1b 5b 32 30 31 7e 04'
 assert expected in paste.read_text(),paste.read_text()
 shutil.copy2(paste,out/'paste.log');print('PASS middle-button bracketed paste',flush=True)
 # Exercise callbacks, including modifier-lock variants and outside release.
 def capture_pixel():
  d.screenshot(str(out/'menu-check.png'));return Image.open(out/'menu-check.png').getpixel((500,350))
 def button(n,down):d.xt.XTestFakeButtonEvent(d.d,n,int(down),0);d.x.XSync(d.d,0)
 def menu():
  d.motion(150,120);d.xt.XTestFakeKeyEvent(d.d,ctrl,1,0);button(2,True)
  _,ms=wait_for(d.menus);time.sleep(.5);d.screenshot(str(out/"menu-open.png"));return ms[-1]
 def release():
  button(2,False);d.xt.XTestFakeKeyEvent(d.d,ctrl,0,0);d.x.XSync(d.d,0)
  wait_for(lambda:not d.menus());time.sleep(.1)
 d.x.XkbLockModifiers.argtypes=[C.c_void_p,C.c_uint,C.c_uint,C.c_uint]
 for locks in (0,2,16,18):
  d.x.XkbLockModifiers(d.d,256,18,locks);d.x.XSync(d.d,0)
  before=capture_pixel();m=menu();x,y=d.position(m);d.motion(x+80,y+82);time.sleep(.5);release()
  assert capture_pixel()!=before,('reverse video failed',locks)
  m=menu();x,y=d.position(m);d.motion(x+80,y+82);time.sleep(.5);release();assert capture_pixel()==before
  menu();d.motion(970,680);release();assert capture_pixel()==before
 d.x.XkbLockModifiers(d.d,256,18,0);d.x.XSync(d.d,0)
 print('PASS menus: selection, reverse-video callback, restore and cancellation with Caps/Num Lock',flush=True)
 d.focus(w);d.type('exit\n');wait_for(lambda:not d.find('Xterm412-check'))
 srv.sendline('echo CONSOLE_ALIVE');srv.expect('CONSOLE_ALIVE');srv.expect('% ')
 print('PASS window and console alive',flush=True)
finally:
 if srv:
  if srv.isalive():
   srv.send('\x1d9')
   try:srv.expect(pexpect.EOF,timeout=5)
   except:pass
  srv.close(force=True)
 stop(t4);stop(xvfb)
 if log.exists():shutil.copy2(log,out/"terminal.log")
 shutil.rmtree(folder);tmp.cleanup()
