"""Sound banks as authored parts, and back byte for byte (doc/ASSET_FORMATS.md 10.1.1).

    codec.py    a bank (`hSPK`) to its programs, layers, effect scripts and sample pool, and back
    sfz.py      programs and layers as SFZ: what a sampler plays, and the bank's exact values as psx_* opcodes
    script.py   the bank's id, effect scripts and any raw pool bytes as text (.sfx)
    bank.py     a bank as its .sfz and .sfx, given its samples' bodies
    adpcm.py    Sony's ADPCM encoder (ENCVAG) and the search for 16-bit input it turns into a stored body
    wav.py      a sample as 16-bit mono WAV, its loop in a `smpl` chunk
    declared.py a declared bank (`sound_bank`) and its sounds (`sound`): decoded to those files, built back from them
"""
