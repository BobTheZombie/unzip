#!/usr/bin/env python3
"""Integration tests for legacy tools and isolated parallel archive jobs."""
import io
from pathlib import Path
import subprocess
import tempfile
import zipfile
ROOT = Path(__file__).resolve().parents[1]
def run(tool, *args, ok=True, **kwargs):
    p = subprocess.run([str(ROOT / tool), *map(str, args)], capture_output=True, **kwargs)
    assert (p.returncode == 0) == ok, (args, p.returncode, p.stderr, p.stdout)
    return p
with tempfile.TemporaryDirectory() as tmp:
    d = Path(tmp)
    payload = bytes(range(256)) * 8192
    archives = []
    for i in range(12):
        archive = d / f"archive {i}.zip"
        with zipfile.ZipFile(archive, 'w', compression=zipfile.ZIP_DEFLATED if i % 2 else zipfile.ZIP_STORED) as z:
            z.writestr('nested/data.bin', payload)
            z.writestr('empty', b'')
            z.writestr('small.txt', f'job {i}')
        archives.append(archive)
    run('unzip', '-t', archives[0])
    assert run('funzip', input=archives[1].read_bytes()).stdout == payload
    run('unzip-parallel', '-j', '4', '-t', *archives)
    output = d / 'output'; output.mkdir()
    run('unzip-parallel', '-j', '4', '-d', output, *archives)
    for i in range(12):
        job = output / f'archive-{i+1:06d}'
        assert (job / 'nested/data.bin').read_bytes() == payload
        assert (job / 'empty').read_bytes() == b''
        assert (job / 'small.txt').read_text() == f'job {i}'
    # Existing job directories fail rather than being overwritten.
    run('unzip-parallel', '-d', output, archives[0], ok=False)
    bad = d / 'broken.zip'; bad.write_bytes(b'not a ZIP archive')
    run('unzip-parallel', '-j', '2', '-t', bad, archives[0], ok=False)
    corrupt = bytearray(archives[0].read_bytes())
    corrupt[30 + len('nested/data.bin') + 4] ^= 1
    bad.write_bytes(corrupt)
    run('unzip-parallel', '-t', bad, ok=False)
    for jobs in ['0', '-1', '257', 'junk', '2x']:
        run('unzip-parallel', '-j', jobs, '-t', archives[0], ok=False)
    run('unzip-parallel', '-t', ok=False)
    # UnZipSFX still links and extracts with the new CRC implementation.
    sfx = d / 'sfx'; sfx.write_bytes((ROOT / 'unzipsfx').read_bytes() + archives[1].read_bytes())
    sfx.chmod(0o755)
    sfxdir = d / 'sfx-output'; sfxdir.mkdir()
    p = subprocess.run([str(sfx), '-o', '-q'], cwd=sfxdir, capture_output=True)
    assert p.returncode in (0, 1), p.stderr
    assert (sfxdir / 'nested/data.bin').read_bytes() == payload
print('Integration: stored/deflate, funzip, SFX, concurrent extraction, failures and isolation passed')
