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
# Ticket 807 / E-RE-SAP235-TSC6A-AUX-001 admits the exact-color block law:
# the two Control refusals disappear (log changes are exactly the two
# removed draw-refused lines, ords 347/355) and the icon renders — the
# settled Control frame CRC moves 30847819 -> 5f2c742c while the other five
# cases keep their settled CRCs; every serialized renderer-section image
# hash moves because the resting-cache aux blocks now decode, with zero
# guest CPU/memory/time change (all stop lines byte-identical).
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
     '2b1f27a179b625fa9587f750a1adc8821cf4c85808933819a4569cc51e6aa11d'),
    ('widgets', [(38, 'lower')], 44, 9801773346,
     'b290227ff5609fc8267b2c2419a5af8c849a93bfa8a7a207ec35ffcb902a24e4',
     '220fccdafb8017b590a019e5a53c8ae2c3f0d531f85361ad6a9335b716c33228'),
    ('pin', [(38, 'middle')], 44, 9903048381,
     '79eba44837a8ebeff51d3e9ba8837a03020d574938b63610ff17a909cdabdae5',
     'b1e462231b3287ee99538870dffc16909ac9613df6341c840258813875bccbf1'),
    ('browse', [(38, 'lower'), (39, 'lower'), (40, 'upper')], 44, 10210850502,
     '8a31ead3fb899aa04fdcc8f5cfb1d0d24b2426361a521c364519cdca5b333300',
     '72fe664a8d56badba72f43572ba2f79d28fd2f8bcc611399f87f3106193a56c8'),
    ('control', [(38, 'lower'), (39, 'middle')], 44, 10123133602,
     '5c0b4e8fc0e5816b291506a1ca012d2bf6db325a99d4ae328de539ee45f9e63b',
     '17709c61e10cdcf0c23792a90d24cf7517350661bf64b0c4970c7642e606affc'),
    ('return', [(38, 'middle'), (44, 'middle')], 46, 10249573536,
     '9f404d6b67adc87b6df9c3f310a33465d944ec53be45c4cbd947b91de3b2e54a',
     '1fd27a58598e5062dd90250cfd6a0f1003952002f8450ab231fd222cf3f0c3e9'),
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
