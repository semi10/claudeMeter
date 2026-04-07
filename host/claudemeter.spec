# -*- mode: python ; coding: utf-8 -*-

a = Analysis(
    ['claudemeter_host.py'],
    pathex=[],
    binaries=[],
    datas=[('claudemeter_config.json', '.')],
    hiddenimports=['serial', 'serial.tools.list_ports'],
    hookspath=[],
    hooksconfig={},
    runtime_hooks=[],
    excludes=[],
    noarchive=False,
)

pyz = PYZ(a.pure)

exe = EXE(
    pyz,
    a.scripts,
    a.binaries,
    a.datas,
    [],
    name='claudemeter_host',
    debug=False,
    bootloader_ignore_signals=False,
    strip=False,
    upx=True,
    console=True,
)
