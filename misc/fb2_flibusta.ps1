param($archive_path="")
if( ! $archive_path ) { Write-Error "Argument `"archive_path`" cannot be empty!"; exit 1 }

function Get-ScriptDirectory
{
   $Invocation = (Get-Variable MyInvocation -Scope 1).Value
   Split-Path $Invocation.MyCommand.Path
}

function Power-Balanced
{
   Write-Output "Switching to balanced power scheme"
   & powercfg -setactive 381b4222-f694-41f0-9685-ff5bb260df2e
}

function Power-High
{
   Write-Output "Switching to high power scheme"
   & powercfg -setactive 8c5e7fda-e8bf-4a96-9a85-a6e23a8c635c
}

# no line wrapping please!
$host.UI.RawUI.BufferSize = new-object System.Management.Automation.Host.Size(512,50)

# -----------------------------------------------------------------------------
# Following variables could be changed
# -----------------------------------------------------------------------------

$name    = "flibusta"
$site    = "http://www.flibusta.net"
$retries = 20
$timeout = 60

$mydir   = Get-ScriptDirectory
$wdir    = Join-Path $mydir ($name + (get-date -format "_yyyyMMdd_HHmmss"))
$adir    = Join-Path $archive_path $name
$glog    = Join-Path $mydir ($name + "_res" + (get-date -format "_yyyyMMdd") + ".log")

# -----------------------------------------------------------------------------
# Main body
# -----------------------------------------------------------------------------

if( $glog ) { Start-Transcript $glog }
Trap { 
    Power-Balanced
	if( $glog ) { Stop-Transcript }; break 
}

Power-High

Write-Output "Downloading $name ..."

$new_archives = 0
$before_dir = @(dir $adir)

& $mydir/libget2 --verbose --library is_$name --retry $retries --timeout $timeout --continue --to $adir --tosql $wdir --config $mydir/libget2.conf 2>&1 | Write-Host

if( $LASTEXITCODE -gt 0 ) { Write-Error "LIBGET error - $LASTEXITCODE !"; Power-Balanced; exit 0 }
if( $LASTEXITCODE -eq 0 ) { Write-Output "No new archives..."; Power-Balanced; exit 0 }

$after_dir = @(dir $adir)
$diff_dir  = Compare-Object $before_dir $after_dir

if( $diff_dir )
{
   $diff_dir | foreach `
   {
      $narc = $_.InputObject
      $warc = Join-Path $adir $narc
      $arc  = Get-ChildItem $warc
      if( ! $arc.ReparsePoint ) { $new_archives = $new_archives + 1 }
   }
}

if( $new_archives -eq 0 ) { Write-Output "Nothing to do..."; Power-Balanced; exit 1 }

& $mydir/lib2inpx "--db-name=$name" `
                  "--process=fb2" `
                  "--read-fb2=all" `
                  "--quick-fix" `
                  "--clean-when-done" `
                  "--follow-links" `
                  "--archives=$adir" `
                  "$wdir" 2>&1 | Write-Host

if( ! $? ) { Write-Error "Unable to build INPX!"; Power-Balanced; exit $LASTEXITCODE }

Power-Balanced

if( $glog ) { Stop-Transcript }
