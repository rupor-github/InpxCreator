param($archive_path="")
if( ! $archive_path ) { Write-Error "Argument `"archive_path`" cannot be empty!"; exit 1 }

function Get-ScriptDirectory
{
   $Invocation = (Get-Variable MyInvocation -Scope 1).Value
   Split-Path $Invocation.MyCommand.Path
}

# -----------------------------------------------------------------------------
# Following variables cound be changed
# -----------------------------------------------------------------------------

# $proxy   = "http://address:port/"
$name    = "librusec"
$site    = "http://lib.rus.ec"
$retries = 10
$tables  = @("libgenrelist", "libbook", "libavtoraliase", "libavtorname", "libavtor", "libgenre", "librate", "libseq", "libseqname", "libfilename")

$mydir   = Get-ScriptDirectory
$wdir    = Join-Path $mydir $name
$adir    = Join-Path $archive_path $name
$glog    = Join-Path $mydir $name"_result.log"

# -----------------------------------------------------------------------------
# Main body
# -----------------------------------------------------------------------------

if( $glog ) { Start-Transcript $glog }
Trap { if( $glog ) { Stop-Transcript }; break }

if( $proxy ) { $env:http_proxy=$proxy }

Write-Output "Downloading $name archives..."

$log = Join-Path $mydir $name"_archives.log"

$before_dir = @(dir $adir)

& $mydir/wget "--progress=dot:mega" `
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
              "$site/all/daily" 2>$null

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
         if( $arc.Length -le 0 )
         {
            # Unfortunatly current wget version does not return proper error code... With 1.12 this clause could be removed
            Write-Output "***Archive $narc is corrupted..."
            Remove-Item $warc
         }
         elseif( $arc.Length -gt 22 )
         {
            Write-Output "--Testing integrity of archive $warc"
            & $mydir/7za t $warc | out-null
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
               & $mydir/7za d $warc "*.*" "-x!*.fb2" | out-null
               if( ! $? ) { Write-Output "Archive $warc is corrupted..."; exit 1 }
            }
         }
      }
   }
}

Write-Output "Downloading $name databases..."

if( Test-Path -Path $wdir ) { Rename-Item -Path $wdir -NewName ($wdir + (get-date -format "_MMddyyyyhhmmss")) }
New-Item -type directory $wdir | out-null

$log = Join-Path $mydir $name"_sql.log"
if( Test-Path -Path $log ) { Remove-Item $log }

$tables | foreach `
{
   $arc  = "lib." + $_ + ".sql.gz"
   $warc = Join-Path $wdir $arc

   & $mydir/wget "--progress=dot:mega" `
                 "--tries=$retries" `
                 "--user-agent=Mozilla/5.0" `
                 "--append-output=$log" `
                 "--directory-prefix=$wdir" `
                 "$site/sql/$arc" 2>$null

   # Unfortunatly current wget version does not return proper error code... With 1.12 following 2 lines could be removed
   if( !(Test-Path -Path $warc) )            { Write-Error "Unable to download $arc !"; exit 1 }
   if( $(Get-ChildItem $warc).Length -le 0 ) { Remove-Item $warc; Write-Error "Unable to download $arc !"; exit 1 }

   & $mydir/7za e "-o$wdir" $warc | out-null
   if( ! $? ) { Write-Error "Database file $arc is corrupted"; exit 1 }
   Remove-Item $warc
}

$log = Join-Path $mydir $name"_inpx.log"

& $mydir/lib2inpx "--db-name=$name" `
                  "--process=fb2" `
                  "--read-fb2=last" `
                  "--quick-fix" `
                  "--db-format=2010-02-06" `
                  "--clean-when-done" `
                  "--archives=$archive_path`;$adir" `
                  "$wdir" | Tee-Object -FilePath $log

if( ! $? ) { Write-Error "Unable to build INPX!"; exit 1 }
if( $glog ) { Stop-Transcript }

