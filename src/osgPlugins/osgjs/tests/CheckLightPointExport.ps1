param(
    [Parameter(Mandatory = $true)][string]$FixtureExecutable,
    [Parameter(Mandatory = $true)][string]$OsgjsPlugin
)
$ErrorActionPreference = 'Stop'
& $FixtureExecutable $OsgjsPlugin
if ($LASTEXITCODE -ne 0) { throw "Native fixture failed: $LASTEXITCODE" }

function Find-Named($Node, $Name) {
    if ($Node -is [array]) {
        foreach ($Child in $Node) { Find-Named $Child $Name }
    } elseif ($Node -is [pscustomobject]) {
        if ($Node.Name -eq $Name) { $Node }
        foreach ($Property in $Node.psobject.Properties) { Find-Named $Property.Value $Name }
    }
}
function Check($Condition, $Message) {
    if (!$Condition) { throw $Message }
}
foreach ($File in @('lightpoints-direct.osgjs', 'lightpoints-gles.osgjs')) {
    $Raw = Get-Content $File -Raw
    Check ($Raw.Contains('"osg.LightPointNode"')) "$File light node type"
    Check ($Raw.Contains('"osg.Sequence"') -and $Raw.Contains('"osg.MultiSwitch"')) "$File animated switch ancestors"
    $Scene = $Raw | ConvertFrom-Json
    $Lights = @(Find-Named $Scene 'BatchedLights')
    Check ($Lights.Count -eq 1) "$File light node identity"
    $Lights = $Lights[0]
    Check ($Lights.LightPoints.Count -eq 5) "$File all light records"
    Check ($Lights.LightPoints[1].On -is [bool] -and !$Lights.LightPoints[1].On) "$File boolean Off"
    Check ($Lights.PointSprite -eq $true -and $Lights.MaxPixelSize -eq 64) "$File node settings"
    Check ($Lights.LightPoints[1].BlendingMode -eq 'ADDITIVE') "$File additive blend"
    Check ($Lights.LightPoints[0].Position[0] -eq 0) "$File local position preserved"
    $Types = @('AzimSector', 'ElevationSector', 'AzimElevationSector', 'ConeSector', 'DirectionalSector')
    for ($i = 0; $i -lt 5; ++$i) {
        Check ($Lights.LightPoints[$i].Sector.Type -eq $Types[$i]) "$File sector $i"
    }
    $Elevation = $Lights.LightPoints[1].Sector
    Check ($Elevation.MinFadeElevation -lt -1.5 -and $Elevation.MaxFadeElevation -gt 1.5) "$File clipped elevation fade"
    $Direction = $Lights.LightPoints[4].Sector
    Check ($Direction.HorizFadeAngle -lt 0.4 -and $Direction.VertFadeAngle -eq 1) "$File independent directional fades"
    $Blink = $Lights.LightPoints[0].BlinkSequence
    Check ($Blink.Pulses.Count -eq 2 -and $Blink.PulsePeriod -eq 1 -and $Blink.PhaseShift -eq 0.25) "$File blink values"
    Check ($Lights.LightPoints[1].BlinkSequence.UniqueID -eq $Blink.UniqueID) "$File shared blink reference"
    $Second = Find-Named $Scene 'SharedSystem'
    Check ($Second.LightPointSystem.UniqueID -eq $Lights.LightPointSystem.UniqueID) "$File shared light system"
    Check ($Second.LightPoints[0].BlinkSequence.SequenceGroup.UniqueID -eq $Blink.SequenceGroup.UniqueID) "$File shared sequence group"
    Check ($Blink.SequenceGroup.BaseTime -eq 12.5) "$File sequence base time"
    Check ($Lights.LightPointSystem.AnimationState -eq 'ANIMATION_RANDOM' -and $Lights.LightPointSystem.Intensity -eq 0.75) "$File light system values"
    $Transform = Find-Named $Scene 'LightPointTransform'
    Check ($Transform.Matrix[12] -eq 10 -and $Transform.Matrix[13] -eq 20 -and $Transform.Matrix[14] -eq 30) "$File parent transform"
    $Bone = Find-Named $Scene 'LightPointBone'
    Check ($Bone.Matrix[12] -eq 1 -and $Bone.Matrix[13] -eq 2 -and $Bone.Matrix[14] -eq 3) "$File bone transform"
    $Empty = Find-Named $Scene 'EmptyLights'
    Check ($null -ne $Empty -and $Empty.LightPoints.Count -eq 0) "$File empty light node"
    Check ($Empty.MaxVisibleDistance2 -gt 3e38) "$File finite native default distance"
}
Write-Output 'Direct and GLES JSON contract, sharing, all sectors, clipped fades, transforms and empty-node checks passed.'
