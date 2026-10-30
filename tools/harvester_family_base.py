"""Shared 1x1 resource-harvester pedestal; wood, stone and crystal."""
import math

FAMILY_ID='compact_harvesters_v1'
MOUNT_HEIGHT=.215
PALETTE_U={'orange':.84375,'steel':.84375,'dark_steel':.59375,
           'wood_bark':.15625,'wood_end':.34375,'crystal':.46875}

def build_base(lathe,orange,steel,dark):
    """Return editable parts. Caller owns scene/material/UV helpers."""
    phase=math.pi/8
    parts=[
        lathe('harvester_plinth',[(.463,0),(.480,.018),(.480,.043)],dark,8,phase=phase),
        lathe('harvester_housing',[(.474,.039),(.474,.055),(.455,.180)],orange,8,phase=phase),
        lathe('harvester_upper_rim',[(.463,.176),(.475,.190),(.475,.214),(.419,.214),(.419,.176)],steel,8,phase=phase,closed=True),
        lathe('harvester_module_deck',[(.421,.179),(.421,.208)],dark,8,phase=phase),
    ]
    for i in range(4):
        bolt=lathe('harvester_bolt_%02d'%i,[(.024,.419),(.024,.459)],dark,8,'X',(0,0,.110),False)
        bolt.rotation_euler.z=i*math.pi/2;parts.append(bolt)
    return parts
