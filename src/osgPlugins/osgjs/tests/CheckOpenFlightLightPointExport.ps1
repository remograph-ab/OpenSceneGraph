param(
    [Parameter(Mandatory = $true)][string]$FixtureExecutable,
    [Parameter(Mandatory = $true)][string]$OpenFlightPlugin,
    [Parameter(Mandatory = $true)][string]$OsgjsPlugin,
    [Parameter(Mandatory = $true)][string]$GlesPlugin
)
$ErrorActionPreference = 'Stop'
& $FixtureExecutable $OpenFlightPlugin $OsgjsPlugin $GlesPlugin
if ($LASTEXITCODE -ne 0) { throw "OpenFlight fixture failed: $LASTEXITCODE" }

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
foreach ($File in @('lightpoints-openflight.osgjs', 'lightpoints-openflight-gles.osgjs')) {
    $Scene = Get-Content $File -Raw | ConvertFrom-Json
    $SystemID = $null
    $Names = @('Inline', 'Steady', 'BadTime', 'Strobe', 'Colors', 'Empty', 'ZeroTime', 'AlwaysOn', 'Off', 'NoAnim')
    foreach ($Name in $Names) {
        $Node = @(Find-Named $Scene $Name)
        Check ($Node.Count -eq 1) "$File $Name present once"
        $Node = $Node[0]
        Check ($Node.LightPoints.Count -eq 2) "$File $Name front/back"
        Check ($Node.MinPixelSize -eq 2 -and $Node.MaxPixelSize -eq 64) "$File $Name pixel clamps"
        Check ([math]::Abs($Node.LightPoints[0].Position[0] - 3.048) -lt 0.0001) "$File $Name feet position"
        Check ([math]::Abs($Node.LightPoints[0].Radius - 0.9144) -lt 0.0001) "$File $Name feet diameter converted to meter radius"
        Check ($Node.LightPoints[0].Intensity -eq 1 -and $Node.LightPoints[1].Intensity -eq 0.5) "$File $Name intensity"
        Check ($Node.LightPoints[0].Sector.Type -eq 'DirectionalSector') "$File $Name sector type"
        Check ($Node.LightPoints[0].Sector.Direction[1] -eq 1 -and $Node.LightPoints[1].Sector.Direction[1] -eq -1) "$File $Name opposite sectors"
        Check ([math]::Abs($Node.LightPoints[0].Sector.HorizLobeAngle - [math]::PI/2) -lt 0.0001) "$File $Name radians"
        if ($null -eq $SystemID) { $SystemID = $Node.LightPointSystem.UniqueID }
        Check ($null -ne $SystemID -and $Node.LightPointSystem.UniqueID -eq $SystemID) "$File $Name inherited shared system"
        if ($Name -in @('Steady', 'BadTime', 'Empty', 'ZeroTime', 'NoAnim')) {
            Check ($null -eq $Node.LightPoints[0].BlinkSequence) "$File $Name steady fallback"
        }
    }
    foreach ($Name in @('Inline', 'Strobe', 'AlwaysOn')) {
        $Node = Find-Named $Scene $Name
        $Blink = $Node.LightPoints[0].BlinkSequence
        Check (($Blink.Pulses[-1].Color -join ',') -eq '1,1,1,1') "$File $Name neutral ON"
        Check ($Blink.PhaseShift -eq 0.25) "$File $Name phase seconds"
        foreach ($Pulse in $Blink.Pulses) { Check ($Pulse.Duration -gt 0) "$File $Name positive duration" }
    }
    $Colors = Find-Named $Scene 'Colors'
    $Front = $Colors.LightPoints[0].BlinkSequence
    $Back = $Colors.LightPoints[1].BlinkSequence
    Check ($Front.Pulses.Count -eq 3 -and $Front.PulsePeriod -eq 1.5) "$File filtered invalid durations"
    Check (($Colors.LightPoints[0].Color -join ',') -eq '1,1,1,1') "$File absolute color neutral base"
    Check (($Front.Pulses[2].Color -join ',') -eq '0,1,0,1') "$File absolute green"
    Check ([math]::Abs($Front.Pulses[0].Color[0] - 128/255) -lt 0.0001) "$File original front ON"
    Check (($Back.Pulses[0].Color -join ',') -eq '1,1,1,1') "$File independent back ON"
    Check ($Front.UniqueID -ne $Back.UniqueID) "$File independent color-change sequences"
    $Off = Find-Named $Scene 'Off'
    Check ($Off.LightPoints[0].BlinkSequence.PulsePeriod -eq 1) "$File negative enabled duration clamped"
    Check (($Off.LightPoints[0].BlinkSequence.Pulses[0].Color -join ',') -eq '0,0,0,0') "$File disabled pulse"
}
Write-Output 'Actual FLT direct/GLES JSON colors, timing, pixel clamps, sectors, units and system inheritance passed.'
