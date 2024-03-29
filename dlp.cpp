#include <HalonMTA.h>
#include <stdio.h>
#include <cstring>
#include <mutex>
#include <syslog.h>
#include <curl/curl.h>
#include <string>

std::mutex m;
std::string cpath;
unsigned short cport = 0;
std::string caddress;

HALON_EXPORT
int Halon_version()
{
	return HALONMTA_PLUGIN_VERSION;
}

HALON_EXPORT
bool Halon_init(HalonInitContext* hic)
{
	HalonConfig *cfg;

	HalonMTA_init_getinfo(hic, HALONMTA_INIT_APPCONFIG, nullptr, 0, &cfg, nullptr);

	const char* path_ = HalonMTA_config_string_get(HalonMTA_config_object_get(cfg, "path"), nullptr);
	if (path_)
		cpath = path_;

	const char* port_ = HalonMTA_config_string_get(HalonMTA_config_object_get(cfg, "port"), nullptr);
	if (port_)
		cport = (unsigned short)strtoul(port_, nullptr, 0);

	const char* address_ = HalonMTA_config_string_get(HalonMTA_config_object_get(cfg, "address"), nullptr);
	if (address_)
		caddress = address_;

	curl_global_init(CURL_GLOBAL_ALL);
	return true;
}

HALON_EXPORT
void Halon_config_reload(HalonConfig* hc)
{
	std::lock_guard<std::mutex> lg(m);
	const char* path_ = HalonMTA_config_string_get(HalonMTA_config_object_get(hc, "path"), nullptr);
	if (path_)
		cpath = path_;
	else
		cpath.clear();

	const char* port_ = HalonMTA_config_string_get(HalonMTA_config_object_get(hc, "port"), nullptr);
	if (port_)
		cport = (unsigned short)strtoul(port_, nullptr, 0);
	else
		cport = 0;

	const char* address_ = HalonMTA_config_string_get(HalonMTA_config_object_get(hc, "address"), nullptr);
	if (address_)
		caddress = address_;
	else
		caddress.clear();
}

HALON_EXPORT
void Halon_cleanup()
{
}

size_t curl_string_writer(char *data, size_t size, size_t nmemb, std::string *out)
{
	out->append((const char*)data, size * nmemb);
	return size * nmemb;
}

void dlp(HalonHSLContext* hhc, HalonHSLArguments* args, HalonHSLValue* ret)
{
	m.lock();
	std::string path = cpath, address = caddress;
	size_t port = cport, connect_timeout = 5, timeout = 0;
	m.unlock();

	FILE* fp;
	const HalonHSLValue* a = HalonMTA_hsl_argument_get(args, 0);

	if (a && HalonMTA_hsl_value_type(a) == HALONMTA_HSL_TYPE_FILE)
		HalonMTA_hsl_value_get(a, HALONMTA_HSL_TYPE_FILE, &fp, nullptr);
	else
	{
		HalonHSLValue* x = HalonMTA_hsl_throw(hhc);
 		HalonMTA_hsl_value_set(x, HALONMTA_HSL_TYPE_EXCEPTION, "File argument is not a File object", 0);
		return;
	}

	a = HalonMTA_hsl_argument_get(args, 1);

	if (a && HalonMTA_hsl_value_type(a) == HALONMTA_HSL_TYPE_ARRAY)
	{
		size_t index = 0;
		HalonHSLValue *k, *v;
		while ((v = HalonMTA_hsl_value_array_get(a, index++, &k)))
		{
			char* string;
			size_t stringl;
			double number;
			if (!HalonMTA_hsl_value_get(k, HALONMTA_HSL_TYPE_STRING, &string, &stringl))
				continue;
			if (std::string(string, stringl) == "address")
			{
				if (!HalonMTA_hsl_value_get(v, HALONMTA_HSL_TYPE_STRING, &string, &stringl))
					continue;
				address = std::string(string, stringl);
			}
			if (std::string(string, stringl) == "port")
			{
				if (!HalonMTA_hsl_value_get(v, HALONMTA_HSL_TYPE_NUMBER, &number, nullptr))
					continue;
				port = (size_t)number;
				path = "";
			}
			if (std::string(string, stringl) == "connect_timeout")
			{
				if (!HalonMTA_hsl_value_get(v, HALONMTA_HSL_TYPE_NUMBER, &number, nullptr))
					continue;
				connect_timeout = (size_t)number;
			}
			if (std::string(string, stringl) == "timeout")
			{
				if (!HalonMTA_hsl_value_get(v, HALONMTA_HSL_TYPE_NUMBER, &number, nullptr))
					continue;
				timeout = (size_t)number;
			}
			if (std::string(string, stringl) == "path")
			{
				if (!HalonMTA_hsl_value_get(v, HALONMTA_HSL_TYPE_STRING, &string, &stringl))
					continue;
				path = std::string(string, stringl);
				port = 0;
			}
		}
	}

	char* out = nullptr;
	size_t r;
	if (a && !HalonMTA_hsl_value_to_json(a, &out, &r))
	{
		HalonHSLValue* x = HalonMTA_hsl_throw(hhc);
 		HalonMTA_hsl_value_set(x, HALONMTA_HSL_TYPE_EXCEPTION, out, 0);
		free(out);
		return;
	}

	CURL *curl = curl_easy_init();
	curl_mime *form = curl_mime_init(curl);
	curl_mimepart *field = curl_mime_addpart(form);
	curl_mime_name(field, "options");
	curl_mime_data(field, out, CURL_ZERO_TERMINATED);
 	free(out);

	field = curl_mime_addpart(form);
	curl_mime_name(field, "rfc822");
	fseek(fp, 0, SEEK_END);
	size_t length = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	curl_mime_data_cb(field, length, (curl_read_callback)fread, (curl_seek_callback)fseek, nullptr, fp);
 
	std::string data;

	if (!port)
	{
		curl_easy_setopt(curl, CURLOPT_UNIX_SOCKET_PATH, (!path.empty()) ? path.c_str() : "/var/run/halon/dlpd.sock");
		curl_easy_setopt(curl, CURLOPT_URL, "http://localhost/v1/scan");
	}
	else
	{
		curl_easy_setopt(curl, CURLOPT_URL, ("http://" + ((!address.empty()) ? address : "127.0.0.1") + ":" + std::to_string(port) + "/v1/scan").c_str());
	}

	curl_easy_setopt(curl, CURLOPT_MIMEPOST, form);
	if (timeout)
		curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeout);
	if (connect_timeout)
		curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, connect_timeout);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_string_writer);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &data);

	CURLcode res = curl_easy_perform(curl);

	long status = 0;
	if (res == CURLE_OK)
		curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
	curl_easy_cleanup(curl);
	curl_mime_free(form);

	if (res != CURLE_OK)
	{
		HalonMTA_hsl_value_set(ret, HALONMTA_HSL_TYPE_ARRAY, nullptr, 0);
		HalonHSLValue *key, *val;
		HalonMTA_hsl_value_array_add(ret, &key, &val);
		HalonMTA_hsl_value_set(key, HALONMTA_HSL_TYPE_STRING, "error", 0);
		HalonMTA_hsl_value_set(val, HALONMTA_HSL_TYPE_STRING, curl_easy_strerror(res), 0);
		return;
	}

	if (status != 200)
	{
		HalonMTA_hsl_value_set(ret, HALONMTA_HSL_TYPE_ARRAY, nullptr, 0);
		HalonHSLValue *key, *val;
		HalonMTA_hsl_value_array_add(ret, &key, &val);
		HalonMTA_hsl_value_set(key, HALONMTA_HSL_TYPE_STRING, "error", 0);
		HalonMTA_hsl_value_set(val, HALONMTA_HSL_TYPE_STRING, data.c_str(), 0);
		return;
	}

 	out = nullptr;
	if (!HalonMTA_hsl_value_from_json(ret, data.c_str(), &out, &r))
	{
		HalonMTA_hsl_value_set(ret, HALONMTA_HSL_TYPE_ARRAY, nullptr, 0);
		HalonHSLValue *key, *val;
		HalonMTA_hsl_value_array_add(ret, &key, &val);
		HalonMTA_hsl_value_set(key, HALONMTA_HSL_TYPE_STRING, "error", 0);
		HalonMTA_hsl_value_set(val, HALONMTA_HSL_TYPE_STRING, out, r);
		free(out);
		return;
	}
}

HALON_EXPORT
bool Halon_hsl_register(HalonHSLRegisterContext* ptr)
{
	HalonMTA_hsl_register_function(ptr, "dlp", &dlp);
	HalonMTA_hsl_module_register_function(ptr, "dlp", &dlp);
	return true;
}
