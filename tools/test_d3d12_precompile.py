"""Exercise bounds checks without using or distributing game assets."""
import struct
import subprocess
import sys
import tempfile
from pathlib import Path

tool = Path(sys.argv[1]).resolve()
with tempfile.TemporaryDirectory(prefix='cg-precompile-test-') as tmp:
    root = Path(tmp)
    cases = [
        ('empty', b'', 2),
        ('no-programs', bytes(96), 1),
        ('out-of-bounds', struct.pack('>8I', 7003, 6, 128, 0, 32, 32,
                                     65536, 48) + bytes(96), 1),
        ('overflow-parameters', struct.pack('>8I', 7004, 6, 128, 0xffffffff,
                                            32, 32, 16, 64) + bytes(96), 1),
    ]
    for name, data, expected in cases:
        source = root/(name+'.bin')
        source.write_bytes(data)
        result = subprocess.run([str(tool), str(source), str(root/'cache')],
                                capture_output=True, timeout=15)
        assert result.returncode == expected, (name, result)
    result = subprocess.run([str(tool), str(source), str(root/'cache'), '4', '4'],
                            capture_output=True, timeout=15)
    assert result.returncode == 2
    assert not list((root/'cache').glob('*.bin'))
    print('PASS empty input, missing programs, truncated microcode, parameter overflow and worker bounds')
