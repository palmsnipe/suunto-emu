#!/bin/sh
# Ticket 801 / E-EMU-SAP235-NAVIGATION-002: isolated, timestamped clicks.
# These are emulator regression observations, not physical-device equivalence.
set -eu
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
     'a69f4b5d4680823619d12427ca0cfd48c043257434d95e0f3eeb007713b64002'),
    ('widgets', [(38, 'lower')], 44, 9801773346,
     '02f9da48b10dd3ac4933a0033c34f14046178d8f30da9200ab174956e2e32bf2',
     '86bfa8520ee691d6287aa2ce48e84889fa258d13653b858859db92771861fed2'),
    ('pin', [(38, 'middle')], 44, 9903048381,
     '79eba44837a8ebeff51d3e9ba8837a03020d574938b63610ff17a909cdabdae5',
     '326172d81898ab69c534302408bfc4d70d3620e7e22317fbc59c0cb388abed95'),
    ('browse', [(38, 'lower'), (39, 'lower'), (40, 'upper')], 44, 10210850502,
     'ebb677cbd1af5cabd9b22cd54b913fadae698f1df1faf56f9e3e3beb2205d322',
     '94a3d55d6cb980cb188b39209f7d7e586087084e1d6472f044e32561c338f310'),
    ('control', [(38, 'lower'), (39, 'middle')], 44, 10123133602,
     '76dd601d9c6501b0766a8d0f2640a3427edfe7dab12620ea1a3977a7318e5ef3',
     'c65ae7ecbb620c4992f328228fd727b3a50c2f17bc7ea3f61336676c538d6a07'),
    ('return', [(38, 'middle'), (44, 'middle')], 46, 10249573536,
     '9f404d6b67adc87b6df9c3f310a33465d944ec53be45c4cbd947b91de3b2e54a',
     '097614f3f1c6d71127abcd9bd7f71a4525b18e0932bc6d8cca4fe97c3932dc25'),
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
    if sha(snapshot) != 'e25c409d8868cd36a6d62c5c9f7da30442c98f9461b19fdd72ac3be1b245c971':
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
