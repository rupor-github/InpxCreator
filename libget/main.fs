module internal LibGet.Driver 

open System
open System.IO
open System.Net
open System.Net.Cache
open System.Text.RegularExpressions
open System.Collections.Generic
open Config

let re_numbers = Regex("\s*([0-9]+)-([0-9]+).zip",RegexOptions.Compiled)

let library = ref "flibusta"
let retry   = ref 3
let dest    = ref Environment.CurrentDirectory
let config  = ref (Path.Combine( Environment.CurrentDirectory, "libget.conf" ))
let usage   = [ "--library", CommandLine.ArgType.String(fun s -> library := s), "(flibusta) name of the library profile";
                "--retry",   CommandLine.ArgType.Int(fun s -> retry := s),      "(3) number of re-tries";
                "--to",      CommandLine.ArgType.String(fun s -> dest := s),    "(current directory) destination directory"; 
                "--config",  CommandLine.ArgType.String(fun s -> config := s),  "(libget.conf) configuration file"; 
              ] |> List.map (fun (sh, ty, desc) -> CommandLine.ArgInfo(sh, ty, desc))

let rec fetchStr(client: WebClient, url: Uri, attempt) = 
    try
       printfn "Downloading index for \"%s\" (#%d) " url.AbsoluteUri attempt
       client.DownloadString url
    with 
    | :? WebException -> if attempt < !retry then fetchStr(client, url, attempt + 1) else reraise()

let rec fetchFile (client: WebClient, url: Uri, file: string, attempt, temp:Option<string> ) = 
    let tmp = match temp with
              | None -> Path.GetTempFileName()
              | Some t -> t
    try
       printfn "Downloading archive \"%s\" (#%d)" url.AbsoluteUri attempt
       client.DownloadFile (url, tmp)
       File.Move (tmp, file)
    with 
    | :? WebException -> if attempt < !retry then 
                            fetchFile(client, url, file, attempt + 1, Some tmp) 
                         else 
                            try File.Delete tmp with _ -> ()
                            reraise()

let rec getArchives dir =
        seq { yield! Directory.EnumerateFiles(dir, "*.zip") |> Seq.map (fun (name) -> Path.GetFileName(name)) 
              for d in Directory.EnumerateDirectories(dir) do
                 yield! getArchives d }

let getLinks client (pattern:Regex) url =
   let index = fetchStr(client, url, 1)
   let matches = pattern.Matches(index)
   let matchToUrl (urlMatch : Match) = urlMatch.Groups.Item(1).Value
   matches |> Seq.cast |> Seq.map matchToUrl

let disect s =
   let m = re_numbers.Match(s)
   (Convert.ToInt32(m.Groups.Item(1).Value), Convert.ToInt32(m.Groups.Item(2).Value))

[<EntryPoint>]
let main _ = 
   CommandLine.ArgParser.Parse (usage, usageText="\nTool to download library updates\nVersion 1.0\n\nUsage: libget.exe [options]\n")
   try 
      let conf = (getConfig !config).libraries |> Array.find (fun r -> r.name = !library)
      printfn "\nProcessing library \"%s\"\n" conf.name

      use client = new WebClient()
      client.CachePolicy <- new RequestCachePolicy(RequestCacheLevel.BypassCache)
      client.Headers.Add("user-agent", "Mozilla/5.0 (Windows; en-US)")

      let last_book = getArchives !dest |> Seq.fold (fun acc elem -> if acc < snd (disect elem) then snd (disect elem) else acc) 0 
      let new_links = getLinks client (Regex(conf.pattern, RegexOptions.Compiled ||| RegexOptions.IgnoreCase)) (new Uri(conf.url)) |> 
                         Seq.choose (fun elem -> if last_book < fst (disect elem) then Some(elem) else None )
      // Downloading                              
      do new_links |> Seq.iter (fun elem -> fetchFile(client,(new Uri(Path.Combine(conf.url, elem))),(Path.Combine(!dest, elem)),1,None))
      let len = Seq.length new_links
      printfn "\nProcessed %d new archive(s)" len
      exit len
   with 
   | :? KeyNotFoundException -> Console.WriteLine("\nUnable to find configuration record for library: " + !library)
                                exit -1
   | :? WebException as wex  -> Console.WriteLine("\nCommunication error: " + wex.Message)
                                exit -2
   | ex ->                      Console.WriteLine("\nUnexpected error: " + ex.Message)
                                exit -3                                        