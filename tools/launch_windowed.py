#!/usr/bin/env python3
"""Launch the local release ROM windowed, without activating RetroArch on macOS."""
from pathlib import Path
import argparse
import subprocess

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('rom', nargs='?', help='ROM to launch')
parser.add_argument('--launch', action='store_true',
                    help='Explicitly request a GUI launch; never use for automated checks')
args = parser.parse_args()
if not args.launch:
    parser.error('GUI launch disabled by default. Use headless checks; --launch requires an explicit user launch request.')

root = Path(__file__).resolve().parents[1]
support = Path.home() / 'Library/Application Support/RetroArch'
exe = Path('/Applications/RetroArch.app/Contents/MacOS/RetroArch')
core = support / 'cores/genesis_plus_gx_libretro.dylib'
rom = Path(args.rom).resolve() if args.rom else root / 'out/release/rom.bin'
for path in (exe, core, rom):
    if not path.is_file():
        raise SystemExit(f'Missing required file: {path}')
config = root / 'build/retroarch-windowed.cfg'
config.parent.mkdir(exist_ok=True)
config.write_text('''video_driver = "gl"
video_fullscreen = "false"
video_windowed_fullscreen = "false"
video_window_custom_size_enable = "true"
video_window_width = "960"
video_window_height = "672"
video_scale = "3"
video_window_save_positions = "false"
config_save_on_exit = "false"
auto_overrides_enable = "false"
savestate_auto_load = "false"
pause_nonactive = "false"
video_shader_enable = "false"
video_smooth = "false"
video_vsync = "true"
video_adaptive_vsync = "false"
video_swap_interval = "1"
video_threaded = "false"
audio_sync = "true"
slowmotion_ratio = "1.000000"
fastforward_ratio = "1.000000"
stdin_cmd_enable = "false"
fps_show = "true"
input_remap_binds_enable = "false"
input_player1_up = "up"
input_player1_down = "down"
input_player1_left = "left"
input_player1_right = "right"
input_player1_start = "enter"
input_player1_y = "z"
input_player1_b = "x"
input_player1_a = "c"
''')
# Launch Services -g explicitly keeps the user's current app in front.
# Do not follow this with app activation, window raising, or synthetic input.
log = root / 'build/retroarch-debug.log'
log.write_text('')
subprocess.run(['/usr/bin/open', '-g', '-n', '-a', str(exe.parents[2]),
    '--stdout', str(log), '--stderr', str(log), '--args',
    '--config', str(config), '-L', str(core), str(rom)], check=True)
print(f'Requested background launch of {rom} with fullscreen disabled, normal speed, and VSync.')
