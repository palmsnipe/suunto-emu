#!/bin/sh
# Ticket 803 / E-EMU-SAP235-EXERCISE-003: tutorial entry and active Timer8 restore.
# These are emulator regression observations, not physical-device equivalence.
set -eu
# Ticket 803 / E-EMU-SAP235-EXERCISE-003: Apollo4 codec 1 snapshot pins.
# Paired attribution changes only section 5 (+17 bytes); old logs are unchanged.
emulator=${SEMU_SDL_EMULATOR:-build/suunto-emu-sdl}
manifest=${SEMU_FIRMWARE_MANIFEST:-tests/private/sapporo-2.35.34.18929/firmware.semu}
if [ ! -f "$manifest" ]; then
    if [ -n "${SEMU_FIRMWARE_MANIFEST:-}" ]; then
        echo 'error: requested Sapporo 2.35 manifest is missing' >&2; exit 2
    fi
    echo 'SKIP exercise entry: private Sapporo 2.35 firmware unavailable'; exit 0
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

# Synthetic compatibility replies remain bounded to 30 total OHR hits.
with tempfile.TemporaryDirectory(prefix='semu-sap235-exercise-') as directory:
    work = pathlib.Path(directory)
    snapshot = pathlib.Path(supplied) if supplied else work / 'main.sems'
    if not supplied:
        cold_env = dict(env, SEMU_SDL_LIVE_TEST='setup-walk',
                        SEMU_SDL_SETUP_WALK_POST='mmlllmlllmmmmmmmmmmmmmmmmmmmmmm')
        run(base + ['--until', 'setup-next', '--max-instructions', '10000000000',
                    '--max-time', '40000000000', '--snapshot-save', str(snapshot)],
            work / 'cold.log', 0, cold_env)
        assert sha(work / 'cold.log') == '14ffff66146dfce6fabbb57ee2f96917431c02061d6c172ffac731b9adaa9aae'
    if sha(snapshot) != 'c56057a902cc8eba7f824c3422c619efa57eea38025d9c25e1e3ea3b01cd618b':
        raise SystemExit('unexpected watchface snapshot; recreate with current emulator')
    replay = work / 'exercise.replay'
    replay.write_text('38000000000 button upper press\n38100000000 button upper release\n'
                      '39000000000 button middle press\n39100000000 button middle release\n')
    cases = [
        ('tutorial', 44000000000, 10282056430, 44000000000,
         'd628a6a89456103eb279254eef5ae8efd823fb48acbcf0e72fcea054a1ccf868',
         '5f75237a5b17590fcb9726ee4cdb2f7f5047cc663fe92a00b7f10fed14bd99c9'),
        ('pulse', 40080000000, 10208679798, 40130748854,
         '4a77dae22f95d86a95edd668bc1850c3121b2d9529580eb67922dc505be0eacf',
         'b467c2dbf04459fa98362a3343c19bc0ee9a3fdb3aa6122315e8e66c6e38352e')]
    for name, end, instructions, stopped_ns, log_sha, image_sha in cases:
        for repeat in [1, 2]:
            log = work / ('%s-%s.log' % (name, repeat))
            image = work / ('%s-%s.sems' % (name, repeat))
            run(base + ['--snapshot-load', str(snapshot), '--input-replay', str(replay),
                        '--max-instructions', '12000000000', '--max-time', str(end),
                        '--snapshot-save', str(image)], log, 3)
            text = log.read_text()
            expected = 'stop=budget pc=0x000e1862 instructions=%s virtual_time_ns=%s' % (instructions, stopped_ns)
            assert text.splitlines()[-1] == expected
            assert sha(log) == log_sha and sha(image) == image_sha
            assert not any(word in text for word in ['machine-reset-request', 'draw-refused', 'compat-refused'])
        for suffix in ['log', 'sems']:
            assert (work / (name + '-1.' + suffix)).read_bytes() == (work / (name + '-2.' + suffix)).read_bytes()
    for repeat in [1, 2]:
        log = work / ('resume-%s.log' % repeat)
        image = work / ('resume-%s.sems' % repeat)
        run(base + ['--snapshot-load', str(work / 'pulse-1.sems'),
                    '--max-instructions', '12000000000', '--max-time', '44000000000',
                    '--snapshot-save', str(image)], log, 3)
        assert image.read_bytes() == (work / 'tutorial-1.sems').read_bytes()
    assert (work / 'resume-1.log').read_bytes() == (work / 'resume-2.log').read_bytes()
    print('PASS Sapporo 2.35 exercise tutorial; active Timer8 restore equals uninterrupted state')
PYTHON
