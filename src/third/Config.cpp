/*
*	Copyright(c) 2020 lutianming email：641471957@qq.com
*
*	Sheeps may be copied only under the terms of the GNU Affero General Public License v3.0
*/

#include "Config.h"
#include <list>
#include <map>
#include <fcntl.h>
#include <string>
#include <fstream>
#include <string.h>
#include <stdlib.h>

const char* default_configfile = "./config.ini";
const char* default_section = "__HOME_SECTION__";
std::map<std::string, void*>* MapConfig = NULL;

static inline bool char_is_blank(int c)
{
	if (c == 32 || c == 9 || c == 0x0a)
		return true;
	return false;
}

static inline int right_blank(char* buf, int len)
{
	char* p = buf + len-1;
	while (char_is_blank(*p)  && p >= buf)
	{
		*p = 0x0;
		p--;
	}
	return int(p - buf) + 1;
}

static inline int left_blank(char* buf, int len)
{
	char* p = buf;
	while (char_is_blank(*p) && p<buf+len)
	{
		p++;
	}
	len = len - int(p - buf);
	memmove(buf, p, len);
	*(buf + len) = 0x0;
	return len;
}

static inline int trim(char* buf, int len)
{
	return left_blank(buf, right_blank(buf, len));
}

#ifdef _MSC_VER
static inline int strncasecmp(const char* input_buffer, const char* s2, register int n)
{
	return _strnicmp(input_buffer, s2, n);
}
#endif

static inline void create_section(char* section, size_t secsize,const char* buf)
{
	memset(section, 0, secsize);
	snprintf(section, secsize, "%s", buf);
	std::map<std::string, char*>* node = new std::map<std::string, char*>;
	MapConfig->insert(std::pair<std::string, void*>(section, (void*)node));
}

static int push_value(char* buf, int len, char* section, size_t secsize)
{
	int n = trim(buf, len);
	if (n == 0)
		return 0;
	
	char* p = strchr(buf, '=');
	if (p == NULL)
	{
		if (*buf == '[' && *(buf + n-1) == ']')
		{
			*(buf + n - 1) = 0x0;
			create_section(section, secsize, buf+1);
		}
		return 0;
	}
	*p = 0x0;
	char* k = buf;
	right_blank(k, int(p - k));

	char* v = p + 1;
	n = left_blank(v, len - int(v - k));
	char* value = (char*)malloc((size_t)n + 1);
	if (value == NULL)
		return 0;
	memcpy(value, v, n);
	*(value + n) = 0x0;

	std::map<std::string, void*>::iterator iter;
	iter = MapConfig->find(section);
	if (iter != MapConfig->end())
	{
		std::map<std::string, void*>* node = (std::map<std::string, void*>*)iter->second;
		std::map<std::string, void*>::iterator it;
		it = node->find(k);
		if (it != node->end())
		{
			free(it->second);
			node->erase(it);
		}
		node->insert(std::pair<std::string, void*>(k, value));
	}
	return 0;
}

static int read_config_file(const char* file) 
{
	if (file == NULL)
		file = default_configfile;
	char buf[1024] = { 0x0 };
	FILE* fhandl = NULL;
#ifdef __WINDOWS__
	int ret = fopen_s(&fhandl, file, "r");
#else
	fhandl = fopen(file, "r");
#endif // __WINDOWS__
	if (fhandl == NULL) return -1;

	char section[64] = { 0x0 };
	create_section(section, sizeof(section), default_section);

	while (true)
	{
		if (fgets(buf, sizeof(buf), fhandl) == NULL) {
			break;
		}
		push_value(buf, (int)strlen(buf), section, sizeof(section));
	}
	
	fclose(fhandl);
	return 0;
}

int config_init(const char* file)
{
	if (MapConfig == NULL)
		MapConfig =  new(std::nothrow) std::map<std::string, void*>;
	return read_config_file(file);
}

int netaddr_in_config(const char* ip, int port, const char* section, const char* key) 
{
	char addr[32] = { 0x0 };
	snprintf(addr, sizeof(addr), "%s:%d", ip, port);

	std::map<std::string, void*>::iterator iter;

	if (section == NULL)
		section = default_section;

	iter = MapConfig->find(section);
	if (iter == MapConfig->end())
		return -1;
	std::map<std::string, void*>* node = (std::map<std::string, void*>*)iter->second;
	iter = node->find(key);
	if (iter == node->end())
		return -2;
	if (strstr((char*)iter->second, addr) == NULL)
		return -3;
	return 0;
}

const char* config_get_string_value(const char* section, const char* key, const char* def)
{
	std::map<std::string, void*>::iterator iter;

	if (section == NULL)
		section = default_section;
	iter = MapConfig->find(section);
	if (iter == MapConfig->end())
		return def;
	std::map<std::string, void*>* node = (std::map<std::string, void*>*)iter->second;
	iter = node->find(key);
	if (iter == node->end())
		return def;
	return (char*)iter->second;
}

int config_get_int_value(const char* section, const char* key, int def)
{
	std::map<std::string, void*>::iterator iter;

	if (section == NULL)
		section = default_section;

	iter = MapConfig->find(section);
	if (iter == MapConfig->end())
		return def;
	std::map<std::string, void*>* node = (std::map<std::string, void*>*)iter->second;
	iter = node->find(key);
	if (iter == node->end())
		return def;
	return  atoi((char*)iter->second);
}

bool config_get_bool_value(const char* section, const char* key, bool def)
{
	std::map<std::string, void*>::iterator iter;

	if (section == NULL)
		section = default_section;

	iter = MapConfig->find(section);
	if (iter == MapConfig->end())
		return def;
	std::map<std::string, void*>* node = (std::map<std::string, void*>*)iter->second;
	iter = node->find(key);
	if (iter == node->end())
		return def;
	if (isdigit(*(char*)(iter->second)))
		return !(0 == atoi((char*)iter->second));
	else
		return strncasecmp((char*)iter->second, "true", 4) == 0;
}