# DLP client plugin

## Installation

Follow the [instructions](https://docs.halon.io/manual/comp_install.html#installation) in our manual to add our package repository and then run the below command.

### Ubuntu

```
apt-get install halon-extras-dlp
```

### RHEL

```
yum install halon-extras-dlp
```

## Configuration
For the configuration schema, see [dlp-app.schema.json](dlp-app.schema.json).

## Exported functions

These functions needs to be [imported](https://docs.halon.io/hsl/structures.html#import) from the `extras://dlp` module path.

### dlp(fp [, options])

Scan the `fp` (File pointer) using the DLP engine. The `options` parameter let you use custom filters and change the behaviour.

**Params**

- fp `File` - The file (eg. a MailMessage File)
- options `array` - Options array

The following options are available in the **options** array.

- path `string` - Path to a the dlpd unix socket. The default is /var/run/halon/dlpd.sock.
- address `string` - Host of the dlpd server.
- port `number` - Port of the dlpd server.
- connect_timeout `number` - The connect timeout. The default is 5 seconds.
- timeout `number` - The (read) timeout.
- id `string` - The log ID to associate with the scan request.
- short_circuit `bool` - Stop on first match. The default is `false`.
- time_limit `number` - Maximum time spent scanning. The default is `0` (no limit).
- recursion_limit `number` - Stop on first match. The default is `9`.
- rules `array` - An array of preconfigured rules to use (string) or custom rules (array).
  - name `string` - The rule name. This property is required.
  - regex `array` - List of text patterns to search for in content (regular expression)
  - filename `array` - List of file names (regular expression)
  - mimetype `array` - List of MIME types (regular expression)
  - md5hash `array` - List of MD5 checksums in hex format
  - sha1hash `array` - List of SHA1 checksums in hex format
  - sha2hash `array` - List of SHA2 checksums in hex format (either 256 or 512)

Do not allow untrusted users to add custom regular expression, since not all regular expressions are safe. All user data should be escaped using pcre_quote() before compiled into a regular expression.

**Example**

```
import { dlp } from "extras://dlp";

echo dlp(
    MIME()
        ->addHeader("Subject", "Eicar Test")
        ->setType("text/virus")
        ->setBody(''X5O!P%@AP[4\PZX54(P^)7CC)7}$EICAR-STANDARD-ANTIVIRUS-TEST-FILE!$H+H*'')
        ->toFile(),
    [
        "id" => "eicar_test",
        "rules" => [
            [ "name" => "EICAR_MD5", "md5hash" => ["44d88612fea8a8f36de82e1278abb02f"] ],
            [ "name" => "EICAR_SHA1", "sha1hash" => ["3395856ce81f2b7382dee72602f798b642f14140"] ],
            [ "name" => "EICAR_SHA2_256", "sha2hash" => ["275a021bbfb6489e54d471899f7db9d1663fc695ec2fe2a2c4538aabf651fd0f"] ],
            [ "name" => "EICAR_SHA2_512", "sha2hash" => ["cc805d5fab1fd71a4ab352a9c533e65fb2d5b885518f4e565e68847223b8e6b85cb48f3afad842726d99239c9e36505c64b0dc9a061d9e507d833277ada336ab"] ],
            [ "name" => "EICAR_REGEX", "regex" => ["/STANDARD-ANTIVIRUS-TEST/"] ],
            [ "name" => "EICAR_SUBJECT_REGEX", "regex" => ["/Eicar Test/"] ],
            [ "name" => "EICAR_MIME", "mimetype" => ["/^text\\/virus$/"] ],
        ]
    ]
);
```

**Returns**

An associative array, with a `result` property containing a list of matched rules (each result item contains a `partid` (refering to the MIME part ID) and a `rule` property which is the name of the rule). If an error occures an `error` property (string) is set contaning the error message.

The following builtin result may be triggered.

- `ERR_UNKNOWN_ERROR` - An unknown error occurred (more details may be available in the log)
- `ERR_PASSWORD_PROTECTED` - The archive is password protected
- `ERR_RECURSION_LIMIT` - The archive is too nested
