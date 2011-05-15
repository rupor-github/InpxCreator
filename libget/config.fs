namespace LibGet

module Config = 

   open System.IO
   open System.Xml
   open System.Text
   open System.Runtime.Serialization
   open System.Runtime.Serialization.Json

   [<DataContract>]
   type Librecord = {
      [<field: DataMember(Name = "name")>]
      name:    string;
      [<field: DataMember(Name = "pattern")>]
      pattern: string;
      [<field: DataMember(Name = "url")>]
      url:     string;
   }
   [<DataContract>]
   type Configuration = { 
      [<field: DataMember(Name = "libraries")>]
      libraries : Librecord array; 
   }

   let internal json<'t> (myObj:'t) =   
      use ms = new MemoryStream() 
      (new DataContractJsonSerializer(typeof<'t>)).WriteObject(ms, myObj) 
      Encoding.Default.GetString(ms.ToArray()) 

   let internal unjson<'t> (jsonString:string)  : 't =  
      use ms = new MemoryStream(ASCIIEncoding.Default.GetBytes(jsonString)) 
      let obj = (new DataContractJsonSerializer(typeof<'t>)).ReadObject(ms) 
      obj :?> 't

   let defConfig = 
      { 
         libraries =  
            [| 
               { name = "flibusta"; pattern = "<a\s+href=\"(f(?:\.fb2)*\.[0-9]+-[0-9]+.zip)\">"; url = "http://www.flibusta.net/daily" };
               { name = "librusec"; pattern = "<a\s+href=\"([0-9]+-[0-9]+.zip)\">";              url = "http://lib.rus.ec/all/"        }; 
            |] 
      }

   let getConfig (file) : Configuration =
      let res = 
         try
            if File.Exists file then
               unjson (File.ReadAllText file )
            else
               File.WriteAllText (file, (json defConfig))
               defConfig
         with 
         | _ -> printf "Error accessing configuration file %s" file
                printf "Using default configuration"
                defConfig
      res
   