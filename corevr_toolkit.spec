# -*- mode: python ; coding: utf-8 -*-
import sys
import os
import glob
from PyInstaller.utils.hooks import Tree

proj = os.path.abspath(os.path.dirname(__file__))
script = os.path.join(proj, 'src', 'ui', 'main.py')

# Collect compiled native python extension if present in src/ui
binaries = []
for f in glob.glob(os.path.join(proj, 'src', 'ui', 'corevr_bridge.*')):
    # add (src, dest) tuple
    binaries.append((f, '.'))

# Include folders config/ and extensions/ into the bundle
datas = [ Tree(os.path.join(proj, 'config'), 'config'), Tree(os.path.join(proj, 'extensions'), 'extensions'), Tree(os.path.join(proj, 'assets'), 'assets') ]

a = Analysis([script],
             pathex=[proj],
             binaries=binaries,
             datas=datas,
             hiddenimports=[],
             hookspath=[],
             runtime_hooks=[],
             win_no_prefer_redirects=False,
             win_private_assemblies=False,
             noarchive=False)

pyz = PYZ(a.pure, a.zipped_data)

# Create windowed/GUI executable (console hidden)
exe = EXE(pyz,
          a.scripts,
          exclude_binaries=True,
          name='corevr_toolkit',
          debug=False,
          bootloader_ignore_signals=False,
          strip=False,
          upx=False,
          icon=os.path.join('assets','icons','app_icon.ico'),
          console=False)

coll = COLLECT(exe,
               a.binaries,
               a.zipfiles,
               a.datas,
               strip=False,
               upx=False,
               name='corevr_toolkit')
