"""Compare every embedded Windows icon frame with the supplied ICO, byte for byte."""
from pathlib import Path
import ctypes, hashlib, json, struct, sys

exe, icon = map(Path, sys.argv[1:3])
k = ctypes.WinDLL('kernel32', use_last_error=True)
k.LoadLibraryExW.argtypes = [ctypes.c_wchar_p, ctypes.c_void_p, ctypes.c_uint32]
k.LoadLibraryExW.restype = ctypes.c_void_p
k.FindResourceW.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_void_p]
k.FindResourceW.restype = ctypes.c_void_p
k.LoadResource.argtypes = [ctypes.c_void_p, ctypes.c_void_p]
k.LoadResource.restype = ctypes.c_void_p
k.LockResource.argtypes = [ctypes.c_void_p]
k.LockResource.restype = ctypes.c_void_p
k.SizeofResource.argtypes = [ctypes.c_void_p, ctypes.c_void_p]
k.SizeofResource.restype = ctypes.c_uint32
k.FreeLibrary.argtypes = [ctypes.c_void_p]
h = k.LoadLibraryExW(str(exe.resolve()), None, 2)
assert h, ctypes.get_last_error()

def resource(name, kind):
    name = ctypes.cast(ctypes.c_wchar_p(name), ctypes.c_void_p) if isinstance(name, str) else ctypes.c_void_p(name)
    r = k.FindResourceW(h, name, ctypes.c_void_p(kind))
    assert r, (name, kind, ctypes.get_last_error())
    return ctypes.string_at(k.LockResource(k.LoadResource(h, r)), k.SizeofResource(h, r))

try:
    original = icon.read_bytes()
    count = struct.unpack_from('<H', original, 4)[0]
    groups = {}
    for name in ['IDI_ICON1', 'IDI_ICON2']:
        group = resource(name, 14)
        assert struct.unpack_from('<H', group, 4)[0] == count
        frames = []
        for i in range(count):
            supplied = struct.unpack_from('<BBBBHHII', original, 6 + 16*i)
            embedded = struct.unpack_from('<BBBBHHIH', group, 6 + 14*i)
            # rc.exe normalizes PNG icons' plane count from 0 to 1.
            assert supplied[:4] == embedded[:4] and supplied[5:7] == embedded[5:7]
            assert embedded[4] in (supplied[4], 1)
            expected = original[supplied[7]:supplied[7]+supplied[6]]
            actual = resource(embedded[7], 3)
            assert actual == expected
            frames.append({'width': supplied[0] or 256, 'height': supplied[1] or 256,
                           'bytes': len(actual), 'sha256': hashlib.sha256(actual).hexdigest()})
        groups[name] = frames
    print(json.dumps({'exe': str(exe), 'icon': str(icon), 'groups': groups, 'all_frames_byte_identical': True}, indent=2))
finally:
    k.FreeLibrary(h)
