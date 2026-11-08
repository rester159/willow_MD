"""Foreground Scroll2 selectors at 2DBF8.
The separate 2EB24 table is Scroll3 parallax, NOT an attract/demo route.
Source: camera initializers 426C0 and 42FAC, shifts at 42FC4/43174.
"""
POINTER_TABLE=0x2DBF8
MODE_TABLE=0x2DC1C

def gameplay_map(program, stage):
    """Decode source selector dimensions; caller handles CPS upward map Y.

    Modes correspond to the masks/shifts in the dispatch at 0x436C2.
    Only stage 0 has also been checked against live Scroll2 RAM so far.
    """
    if not 0 <= stage < 8: raise ValueError('stage must be 0..7')
    pointer=int.from_bytes(program[POINTER_TABLE+stage*4:POINTER_TABLE+stage*4+4],'big')
    mode=program[MODE_TABLE+stage]
    columns,rows=((16,16),(32,8),(64,32),(256,2))[mode]
    grid=list(program[pointer:pointer+columns*rows])
    assert len(grid)==columns*rows
    return {'pointer':pointer,'mode':mode,'columns':columns,'rows':rows,'chunk_ids':grid}

def gameplay_stage0(program):
    # The normal camera initializer loads this pointer table at 426C0.
    assert program[0x426c0:0x426c6]==bytes.fromhex('227c0002dbf8')
    pointer=int.from_bytes(program[POINTER_TABLE:POINTER_TABLE+4],'big')
    assert pointer==0x2dc24 and program[MODE_TABLE]==0
    grid=list(program[pointer:pointer+256])
    assert grid[240:245]==[3,4,5,6,8]
    assert not any(grid[:224])
    width=(max(i%16 for i,c in enumerate(grid) if c)+1)*256
    return grid,width,512
