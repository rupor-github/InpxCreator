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
$retries = 10

$mydir   = Get-ScriptDirectory
$wdir    = Join-Path $mydir $name
$adir    = Join-Path $archive_path $name
$glog    = Join-Path $mydir $name"_result.log"

# -----------------------------------------------------------------------------
# Main body
# -----------------------------------------------------------------------------

$zip = where.exe "7z.exe" 2>$null
if( -not $zip )
{
   $zip = where.exe "7za.exe" 2>$null
   if( -not $zip )
   {
      throw "7z archiver not found in the path: 7z.exe or 7za.exe!"
   }
}
if( $zip.Length -gt 1 )
{
   $zip = $zip[ 0 ]
}

$wget = where.exe "wget-1.12.exe" 2>$null
if( -not $wget )
{
   throw "GNU wget is not found in the path: wget.exe!"
}

$tmp = [System.IO.Path]::GetTempFileName()

if( $glog ) { Start-Transcript $glog }
Trap { if( $glog ) { Stop-Transcript }; break }

if( $proxy ) { $env:http_proxy=$proxy }

Write-Output "Downloading $name archives..."

$log = Join-Path $mydir $name"_archives.log"

$before_dir = @(dir $adir)

$need_cleanup = 0

& $wget "--progress=dot:mega" `
        "--tries=$retries" `
        "--user-agent=Mozilla/5.0" `
        "--output-file=$log" `
        "--recursive" `
        "--no-directories" `
        "--no-parent" `
        "--no-remove-listing" `
        "--accept=*.zip" `
        "--directory-prefix=$adir" `
        "--no-clobber" `
        "--no-cache" `
        "-e robots=off" `
        "$site/all" 2>$null

if( ! $? ) { Write-Error "WGET error - $LASTEXITCODE !"; $need_cleanup = $LASTEXITCODE }

$after_dir = @(dir $adir)

$diff_dir  = Compare-Object $before_dir $after_dir

if( $diff_dir )
{
   $diff_dir | foreach `
   {
      $narc = $_.InputObject
      $warc = Join-Path $adir $narc
      $arc  = Get-ChildItem $warc

      if( ! $arc.ReparsePoint )
      {
         if( ($need_cleanup -gt 0) -or ($arc.Length -le 0) )
         {
            Write-Output "***Archive $narc is corrupted..."
            Remove-Item $warc
         }
         elseif( $arc.Length -gt 22 )
         {
            Write-Output "--Testing integrity of archive $warc"
            & $zip "t" "$warc" | Tee-Object -FilePath $tmp
            if( ! $? )
            {
               Write-Output "***Archive $warc is corrupted..."
               Remove-Item $warc
               continue
            }
            else
            {
               # remove non-fb2 content
               Write-Output "--Removing non-FB2 books in archive $warc"
               & $zip "d" "$warc" "*.*" "-w" "-x!*.fb2" | Tee-Object -FilePath $tmp
               if( ! $? ) { Write-Error "Archive $warc is corrupted..."; exit $LASTEXITCODE }
            }
         }
      }
   }
}

if( $need_cleanup -gt 0 ) { exit $need_cleanup }

Write-Output "Downloading $name databases..."

if( Test-Path -Path $wdir ) { Rename-Item -Path $wdir -NewName ($wdir + (get-date -format "_MMddyyyyhhmmss")) }
New-Item -type directory $wdir | out-null

$log = Join-Path $mydir $name"_sql.log"
if( Test-Path -Path $log ) { Remove-Item $log }

& $wget "--progress=dot:mega" `
        "--tries=$retries" `
        "--user-agent=Mozilla/5.0" `
        "--output-file=$log" `
        "--recursive" `
        "--no-directories" `
        "--accept=*.gz" `
        "--no-parent" `
        "--no-remove-listing" `
        "--directory-prefix=$wdir" `
        "--no-clobber" `
        "--no-cache" `
        "-e robots=off" `
        "$site/sql" 2>$null

if( ! $? ) { Write-Error "WGET error - $LASTEXITCODE !"; exit $LASTEXITCODE }

@(dir $wdir) | foreach `
{
   $narc = $_
   $warc = Join-Path $wdir $narc

   if( $warc.Length -le 0 ) { Remove-Item $warc; Write-Error "Unable to download $narc !"; exit 1 }

   & $zip "e" "-o$wdir" $warc | Tee-Object -FilePath $tmp
   if( ! $? ) { Write-Error "Database file $narc is corrupted"; exit $LASTEXITCODE }

   Remove-Item $warc
}

$log = Join-Path $mydir $name"_inpx.log"

& $mydir/lib2inpx "--db-name=$name" `
                  "--process=fb2" `
                  "--read-fb2=last" `
                  "--quick-fix" `
                  "--db-format=2010-04-11" `
                  "--clean-when-done" `
                  "--archives=$archive_path`;$adir" `
                  "$wdir" | Tee-Object -FilePath $tmp

if( ! $? ) { Write-Error "Unable to build INPX!"; exit $LASTEXITCODE }
if( $glog ) { Stop-Transcript }

Remove-Item $tmp | out-null
