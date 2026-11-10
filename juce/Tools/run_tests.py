#!/usr/bin/env python3
"""Build/run processor regressions against the Projucer Release shared library."""
import argparse
import os
import platform
import re
import shlex
import subprocess
import xml.etree.ElementTree as ET
from pathlib import Path

root = Path(__file__).resolve().parents[2]
project = root / 'juce/Builds/MacOSX'
build = project / 'build'
arch = platform.machine()
settings = ET.parse(root / 'juce/S-MU2000.jucer').getroot()
project_name = settings.attrib['name']
product_name = next(c.attrib.get('targetName',project_name) for c in settings.iter('CONFIGURATION') if c.attrib.get('name')=='Release')
objects = build / f'{project_name}.build/Release/{project_name} - Shared Code.build/Objects-normal/{arch}'
responses = [p for p in objects.glob('*common-args.resp') if '-std=c++20' in p.read_text()]
if not responses:
    raise SystemExit('Build the Release Shared Code target with Xcode first')
flags = shlex.split(responses[0].read_text())
frameworks = sorted(set(re.findall(r'([A-Z][A-Za-z0-9]+)\.framework',
                                  (project / f'{project_name}.xcodeproj/project.pbxproj').read_text())))
out = root / 'build/juce-tests'
out.mkdir(parents=True, exist_ok=True)
parser = argparse.ArgumentParser()
parser.add_argument('tests', nargs='*', default=['DirectAudioTests', 'StateRestoreTests', 'EditorSizeTests', 'LcdLayoutTests', 'AudioInputTests', 'NativePanelTests'])
args = parser.parse_args()
for name in args.tests:
    command = ['xcrun', 'clang++', *flags, '-UNDEBUG', '-arch', arch,
               '-include', str(root / 'juce/JuceLibraryCode/JucePluginDefines.h'),
               str(root / f'juce/Tests/{name}.cpp'),
               str(build / f'Release/lib{product_name}.a'), '-o', str(out / name)]
    for framework in frameworks:
        command += ['-framework', framework]
    print('Building', name, flush=True)
    subprocess.run(command, cwd=project, check=True)
    print('Running', name, flush=True)
    env = {**os.environ, "S_MU2000_ROMS": os.environ.get("S_MU2000_ROMS", str(root / "roms"))}
    arguments = [str(out / name)]
    if name in ('SequencePlayerTests', 'R36Tests'): arguments.append(str(out))
    if name == 'R36AudioTests': arguments.append(str(out / 'player-b-only.r36'))
    subprocess.run(arguments, cwd=root, check=True, env=env)
