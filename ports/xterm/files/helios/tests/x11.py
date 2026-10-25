# SPDX-License-Identifier: MIT
"""Small Xlib/XTest driver for an isolated test display; no desktop input."""
import ctypes as C,time
class Display:
    def __init__(self,name):
        self.x=C.CDLL('libX11.so.6');self.xt=C.CDLL('libXtst.so.6')
        self.x.XOpenDisplay.restype=C.c_void_p
        self.d=self.x.XOpenDisplay(name.encode());assert self.d,'Cannot open '+name
        self.x.XDefaultRootWindow.argtypes=[C.c_void_p];self.x.XDefaultRootWindow.restype=C.c_ulong
        self.root=self.x.XDefaultRootWindow(self.d)
        self.x.XQueryTree.argtypes=[C.c_void_p,C.c_ulong,C.POINTER(C.c_ulong),C.POINTER(C.c_ulong),C.POINTER(C.POINTER(C.c_ulong)),C.POINTER(C.c_uint)]
        self.x.XFetchName.argtypes=[C.c_void_p,C.c_ulong,C.POINTER(C.c_char_p)]
        self.x.XSetInputFocus.argtypes=[C.c_void_p,C.c_ulong,C.c_int,C.c_ulong]
        self.x.XSync.argtypes=[C.c_void_p,C.c_int]
        self.x.XResizeWindow.argtypes=[C.c_void_p,C.c_ulong,C.c_uint,C.c_uint]
        self.x.XStringToKeysym.argtypes=[C.c_char_p];self.x.XStringToKeysym.restype=C.c_ulong
        self.x.XKeysymToKeycode.argtypes=[C.c_void_p,C.c_ulong];self.x.XKeysymToKeycode.restype=C.c_uint
        self.x.XGetKeyboardMapping.argtypes=[C.c_void_p,C.c_uint,C.c_int,C.POINTER(C.c_int)]
        self.x.XGetKeyboardMapping.restype=C.POINTER(C.c_ulong)
        self.x.XFree.argtypes=[C.c_void_p]
        self.xt.XTestFakeKeyEvent.argtypes=[C.c_void_p,C.c_uint,C.c_int,C.c_ulong]
    def find(self,title):
        def visit(w):
            name=C.c_char_p();self.x.XFetchName(self.d,w,C.byref(name))
            text=name.value.decode(errors='replace') if name.value else ''
            if name:self.x.XFree(name)
            if title in text:return w
            root=C.c_ulong();parent=C.c_ulong();children=C.POINTER(C.c_ulong)();n=C.c_uint()
            self.x.XQueryTree(self.d,w,C.byref(root),C.byref(parent),C.byref(children),C.byref(n))
            ids=[children[i] for i in range(n.value)]
            if children:self.x.XFree(children)
            for child in ids:
                found=visit(child)
                if found:return found
        return visit(self.root)
    def focus(self,w):self.x.XSetInputFocus(self.d,w,1,0);self.x.XSync(self.d,0)
    def key(self,name):
        sym=ord(name) if len(name)==1 else self.x.XStringToKeysym(name.encode())
        code=self.x.XKeysymToKeycode(self.d,sym);assert code,name
        count=C.c_int();mapping=self.x.XGetKeyboardMapping(self.d,code,1,C.byref(count))
        shift=mapping[0]!=sym and count.value>1 and mapping[1]==sym
        self.x.XFree(mapping)
        shiftcode=self.x.XKeysymToKeycode(self.d,self.x.XStringToKeysym(b'Shift_L'))
        if shift:self.xt.XTestFakeKeyEvent(self.d,shiftcode,1,0)
        self.xt.XTestFakeKeyEvent(self.d,code,1,0);self.xt.XTestFakeKeyEvent(self.d,code,0,0)
        if shift:self.xt.XTestFakeKeyEvent(self.d,shiftcode,0,0)
        self.x.XSync(self.d,0)
    def type(self,text):
        for ch in text:
            self.key('Return' if ch in '\r\n' else ch)
            time.sleep(.03)
    def resize(self,w,width,height):self.x.XResizeWindow(self.d,w,width,height);self.x.XSync(self.d,0)
    def screenshot(self,path,width=1000,height=700):
        from PIL import Image
        class XImage(C.Structure):
            _fields_=[('width',C.c_int),('height',C.c_int),('xoffset',C.c_int),('format',C.c_int),('data',C.c_void_p),
                      ('byte_order',C.c_int),('bitmap_unit',C.c_int),('bitmap_bit_order',C.c_int),('bitmap_pad',C.c_int),
                      ('depth',C.c_int),('bytes_per_line',C.c_int),('bits_per_pixel',C.c_int)]
        self.x.XGetImage.argtypes=[C.c_void_p,C.c_ulong,C.c_int,C.c_int,C.c_uint,C.c_uint,C.c_ulong,C.c_int]
        self.x.XGetImage.restype=C.POINTER(XImage)
        p=self.x.XGetImage(self.d,self.root,0,0,width,height,C.c_ulong(-1),2);assert p
        v=p.contents;assert v.bits_per_pixel==32
        data=C.string_at(v.data,v.bytes_per_line*v.height)
        Image.frombytes('RGB',(v.width,v.height),data,'raw','BGRX',v.bytes_per_line).save(path)
        self.x.XDestroyImage.argtypes=[C.POINTER(XImage)];self.x.XDestroyImage(p)

class Attributes(C.Structure):
    _fields_=[('x',C.c_int),('y',C.c_int),('width',C.c_int),('height',C.c_int),
      ('border_width',C.c_int),('depth',C.c_int),('visual',C.c_void_p),('root',C.c_ulong),
      ('klass',C.c_int),('bit_gravity',C.c_int),('win_gravity',C.c_int),('backing_store',C.c_int),
      ('backing_planes',C.c_ulong),('backing_pixel',C.c_ulong),('save_under',C.c_int),
      ('colormap',C.c_ulong),('map_installed',C.c_int),('map_state',C.c_int),
      ('all_event_masks',C.c_long),('your_event_mask',C.c_long),('do_not_propagate_mask',C.c_long),
      ('override_redirect',C.c_int),('screen',C.c_void_p)]

class Desktop(Display):
    def __init__(self,name):
        super().__init__(name)
        self.x.XGetWindowAttributes.argtypes=[C.c_void_p,C.c_ulong,C.POINTER(Attributes)]
        self.x.XTranslateCoordinates.argtypes=[C.c_void_p,C.c_ulong,C.c_ulong,C.c_int,C.c_int,C.POINTER(C.c_int),C.POINTER(C.c_int),C.POINTER(C.c_ulong)]
        self.xt.XTestGrabControl.argtypes=[C.c_void_p,C.c_int]
        self.xt.XTestGrabControl(self.d,1)
        self.xt.XTestFakeMotionEvent.argtypes=[C.c_void_p,C.c_int,C.c_int,C.c_int,C.c_ulong]
        self.xt.XTestFakeButtonEvent.argtypes=[C.c_void_p,C.c_uint,C.c_int,C.c_ulong]
        self.x.XSync(self.d,0)
    def children(self,w):
        root=C.c_ulong();parent=C.c_ulong();children=C.POINTER(C.c_ulong)();n=C.c_uint()
        self.x.XQueryTree(self.d,w,C.byref(root),C.byref(parent),C.byref(children),C.byref(n))
        result=[children[i] for i in range(n.value)]
        if children:self.x.XFree(children)
        return result
    def attrs(self,w):
        a=Attributes();assert self.x.XGetWindowAttributes(self.d,w,C.byref(a));return a
    def position(self,w):
        x=C.c_int();y=C.c_int();child=C.c_ulong()
        self.x.XTranslateCoordinates(self.d,w,self.root,0,0,C.byref(x),C.byref(y),C.byref(child))
        return x.value,y.value
    def named(self,text,w=None):
        found=[]
        for child in self.children(w or self.root):
            name=C.c_char_p();self.x.XFetchName(self.d,child,C.byref(name))
            if name.value and text in name.value.decode(errors='replace'):found.append(child)
            if name:self.x.XFree(name)
            found+=self.named(text,child)
        return found
    def motion(self,x,y):
        self.xt.XTestFakeMotionEvent(self.d,-1,x,y,0);self.x.XSync(self.d,0)
    def button(self,down):
        self.xt.XTestFakeButtonEvent(self.d,1,int(down),0);self.x.XSync(self.d,0)
    def click(self,x,y):
        self.motion(x,y);self.button(True);time.sleep(.06);self.button(False)
    def chord(self,modifier,key):
        code=self.x.XKeysymToKeycode(self.d,self.x.XStringToKeysym(modifier.encode()))
        self.xt.XTestFakeKeyEvent(self.d,code,1,0);self.key(key)
        self.xt.XTestFakeKeyEvent(self.d,code,0,0);self.x.XSync(self.d,0)
    def menus(self):
        # FVWM creates its own menus without override_redirect. Unlike
        # managed frames they have no child windows.
        return [w for w in self.children(self.root) if (lambda a:a.map_state==2 and a.width>100 and a.height>100)(self.attrs(w)) and not self.children(w)]

def wait_for(check,timeout=8):
    start=time.monotonic()
    while time.monotonic()-start<timeout:
        result=check()
        if result:return True,result
        time.sleep(.05)
    raise AssertionError("Timed out waiting for an X11 condition")
