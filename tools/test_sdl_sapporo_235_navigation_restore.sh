#!/bin/sh
# Ticket 801 / E-EMU-SAP235-NAVIGATION-002: isolated, timestamped clicks.
# These are emulator regression observations, not physical-device equivalence.
set -eu
# Ticket 803 / E-EMU-SAP235-EXERCISE-003: Apollo4 codec 1 snapshot pins.
# 803 attribution changed only section 5 (+17 bytes); logs were unchanged.
# Ticket 805 / E-RE-SAP235-WIDGET-SCALE-001 removes the Widgets scale refusal.
# Its three pins change only GPU/renderer publication counters; pixels and
# guest CPU/memory/time remain identical. The two Control 20x32 refusals remain.
# Ticket 806 admits the 20x32 shape: the Control log pin moves again because
# the two refusals now name the first unsupported block (2,0 auxiliary bits)
# instead of the shape; count, ordinals, times and every other line are
# unchanged and the control snapshot pin is byte-identical (zero writes
# either way). The icon's aux-bit blocks remain unsupported.
emulator=${SEMU_SDL_EMULATOR:-build/suunto-emu-sdl}
manifest=${SEMU_FIRMWARE_MANIFEST:-tests/private/sapporo-2.35.34.18929/firmware.semu}
if [ ! -f "$manifest" ]; then
    if [ -n "${SEMU_FIRMWARE_MANIFEST:-}" ]; then
        echo 'error: requested Sapporo 2.35 manifest is missing' >&2; exit 2
    fi
    echo 'SKIP restored navigation: private Sapporo 2.35 firmware unavailable'; exit 0
fi
if [ ! -x "$emulator" ]; then
    echo 'error: SDL emulator unavailable (run make sdl)' >&2; exit 2
fi
python3 - "$emulator" "$manifest" "${SEMU_SDL_TEST_SNAPSHOT:-}" <<'PYTHON'
import hashlib, os, pathlib, subprocess, sys, tempfile

emulator, manifest, supplied = sys.argv[1:]
env = dict(os.environ, SDL_VIDEODRIVER='dummy')
# Ambient setup-walk/observer options must not inject input into replay cases.
for key in list(env):
    if key.startswith('SEMU_SDL_'):
        del env[key]
base = [emulator, 'run', '--profile', 'sapporo-2.35.34', '--firmware', manifest]
for layer in ['production-data', 'ohr-startup', 'gps-startup', 'gps-reopen', 'gps-awake']:
    base += ['--layer', 'sapporo-2.35-' + layer]
subprocess.run([emulator, 'validate', '--profile', 'sapporo-2.35.34',
                '--firmware', manifest], check=True, timeout=30, env=env)

def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def run(args, log, expected, child_env=env):
    with log.open('wb') as out:
        result = subprocess.run(args, env=child_env, stdout=out,
                                stderr=subprocess.STDOUT, timeout=900)
    if result.returncode != expected:
        raise SystemExit('%s: expected exit %s, got %s\n%s' %
                         (log.name, expected, result.returncode, log.read_text()))

# name, clicks in seconds, end time, terminal instruction count, full log/image
# hashes. Frame generation/CRC are documented in E-EMU-SAP235-NAVIGATION-002.
cases = [
    ('exercise', [(38, 'upper')], 44, 9809899994,
     '9d6a6202c88727b0c287ddb8cff12b4f8dae095d67c6dec7e3eb056aab9ff855',
     '19f7f8352bcd9fbf867b28b31cbd876a3b233fd05093b9f37caaa945da7842f8'),
    ('widgets', [(38, 'lower')], 44, 9801773346,
     'b290227ff5609fc8267b2c2419a5af8c849a93bfa8a7a207ec35ffcb902a24e4',
     '10151154bba5a724bedf4c9fa3f4943dd44931f5b06fb48180642acc59d681c6'),
    ('pin', [(38, 'middle')], 44, 9903048381,
     '79eba44837a8ebeff51d3e9ba8837a03020d574938b63610ff17a909cdabdae5',
     '99def5985da8ff820d994cee52d88ae18111f4138a8fa42345021dd10a92dd43'),
    ('browse', [(38, 'lower'), (39, 'lower'), (40, 'upper')], 44, 10210850502,
     '8a31ead3fb899aa04fdcc8f5cfb1d0d24b2426361a521c364519cdca5b333300',
     '6d6fa414565e60fa9401a5402ee66b11162a19ce4c89a620f24e54043753ea08'),
    ('control', [(38, 'lower'), (39, 'middle')], 44, 10123133602,
     'caa658680323b8a0a44620e2a69e79280976edcad1c5ccbe2c9cfb3403974380',
     '80cce27096848e339c43a929f7580d91d2832950a752d2cce861d7235c66a9fc'),
    ('return', [(38, 'middle'), (44, 'middle')], 46, 10249573536,
     '9f404d6b67adc87b6df9c3f310a33465d944ec53be45c4cbd947b91de3b2e54a',
     '3de4d7112f4c4c282547332b315e06afa7f20071a2bf673df9b2eae968e7c85f'),
 ]
with tempfile.TemporaryDirectory(prefix='semu-sap235-nav-') as directory:
    work = pathlib.Path(directory)
    snapshot = pathlib.Path(supplied) if supplied else work / 'main.sems'
    if not supplied:
        cold_env = dict(env, SEMU_SDL_LIVE_TEST='setup-walk',
                        SEMU_SDL_SETUP_WALK_POST='mmlllmlllmmmmmmmmmmmmmmmmmmmmmm')
        run(base + ['--until', 'setup-next', '--max-instructions', '10000000000',
                    '--max-time', '40000000000', '--snapshot-save', str(snapshot)],
            work / 'cold.log', 0, cold_env)
        if sha(work / 'cold.log') != '14ffff66146dfce6fabbb57ee2f96917431c02061d6c172ffac731b9adaa9aae':
            raise SystemExit('cold navigation prefix drifted')
    if sha(snapshot) != 'c56057a902cc8eba7f824c3422c619efa57eea38025d9c25e1e3ea3b01cd618b':
        raise SystemExit('unexpected watchface snapshot; recreate with the current emulator')
    for name, clicks, end, instructions, log_sha, image_sha in cases:
        replay = work / (name + '.replay')
        replay.write_text(''.join('%d button %s %s\n' %
            (seconds * 1000000000 + delta, key, action)
            for seconds, key in clicks
            for delta, action in [(0, 'press'), (100000000, 'release')]))
        for repeat in [1, 2]:
            log = work / ('%s-%s.log' % (name, repeat))
            image = work / ('%s-%s.sems' % (name, repeat))
            run(base + ['--snapshot-load', str(snapshot), '--input-replay', str(replay),
                        '--max-instructions', '12000000000', '--max-time', str(end * 1000000000),
                        '--snapshot-save', str(image)], log, 3)
            expected = 'stop=budget pc=0x000e1862 instructions=%s virtual_time_ns=%s' % (
                instructions, end * 1000000000)
            if log.read_text().splitlines()[-1] != expected or sha(log) != log_sha or sha(image) != image_sha:
                raise SystemExit('%s pass %s: navigation checkpoint drifted' % (name, repeat))
        for suffix in ['log', 'sems']:
            if (work / (name + '-1.' + suffix)).read_bytes() != (work / (name + '-2.' + suffix)).read_bytes():
                raise SystemExit(name + ': repeated navigation differs')
        print('PASS Sapporo 2.35 restored navigation: ' + name)
PYTHON
