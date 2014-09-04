param($archive_path="")
if( ! $archive_path ) { Write-Error "Argument `"archive_path`" cannot be empty!"; exit 1 }

function Get-ScriptDirectory
{
   $Invocation = (Get-Variable MyInvocation -Scope 1).Value
   Split-Path $Invocation.MyCommand.Path
}

# -----------------------------------------------------------------------------
# Following variables could be changed
# -----------------------------------------------------------------------------

# $proxy   = "http://host:port"
$name    = "librusec"
$site    = "http://lib.rus.ec"
$retries = 20
$timeout = 60

$mydir   = Get-ScriptDirectory
$wdir    = Join-Path $mydir ($name + (get-date -format "_yyyyMMdd_HHmmss"))
$adir    = Join-Path $archive_path $name
$glog    = Join-Path $mydir ($name + "_res" + (get-date -format "_yyyyMMdd") + ".log")

# -----------------------------------------------------------------------------
# Main body
# -----------------------------------------------------------------------------

$tmp = [System.IO.Path]::GetTempFileName()

if( $glog ) { Start-Transcript $glog }
Trap { if( $glog ) { Stop-Transcript }; break }

if( $proxy ) { $env:http_proxy=$proxy }

Write-Output "Downloading $name ..."

$new_archives = 0
$before_dir = @(dir $adir)

& $mydir/libget2 --library $name --retry $retries --timeout $timeout --continue --to $adir --tosql $wdir --config $mydir/libget2.conf 2>&1 | Tee-Object -FilePath $tmp

if( $LASTEXITCODE -lt 0 ) { Write-Error "LIBGET error - $LASTEXITCODE !"; exit 0 }

# -----------------------------------------------------------------------------
# Temporary leftovers
# -----------------------------------------------------------------------------
# Remove-Item (Join-Path $wdir "libavtoraliase.sql") | out-null
# Remove-Item (Join-Path $wdir "libgenrelist.sql") | out-null
# -----------------------------------------------------------------------------

if( $LASTEXITCODE -eq 0 ) { Write-Output "No new archives..."; exit 0 }

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

if( $new_archives -eq 0 ) { Write-Output "Nothing to do..."; exit 1 }

& $mydir/lib2inpx "--db-name=$name" `
                  "--process=fb2" `
                  "--read-fb2=all" `
                  "--quick-fix" `
                  "--db-format=2011-11-06" `
                  "--clean-authors" `
                  "--inpx-format=2.x" `
                  "--clean-when-done" `
                  "--follow-links" `
                  "--archives=$adir" `
                  "$wdir" 2>&1 | Tee-Object -FilePath $tmp

if( ! $? ) { Write-Error "Unable to build INPX!"; exit $LASTEXITCODE }
if( $glog ) { Stop-Transcript }

Remove-Item $tmp | out-null
