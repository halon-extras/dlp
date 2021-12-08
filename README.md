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

### dlp(fp [, options])

Scan the `fp` (File pointer) using the DLP engine. The `option` paramter let you use custom filters and change the behaviour. On error an associative array with the `error` property is returned.

**Params**

- namespace `File` - The file (eg. a MailMessage File)
- options `array` - Options array
