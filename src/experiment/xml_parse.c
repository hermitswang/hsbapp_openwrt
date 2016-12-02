
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include <libxml/xmlmemory.h>
#include <libxml/parser.h>

#define HSB_CONF_FILE	"hsb.xml"

static int parse_hsb_config(char *file)
{
	xmlDocPtr doc;
	xmlNodePtr cur, dev;
	xmlChar *id, *key;

	doc = xmlParseFile(file);
	if (!doc)
		return -1;

	cur = xmlDocGetRootElement(doc);
	if (!cur) {
		printf("root empty\n");
		return -2;
	}

	printf("root <%s>\n", cur->name);

	cur = cur->xmlChildrenNode;
	while (cur) {
		if (0 != xmlStrcmp(cur->name, (const xmlChar *)"device")) {
			cur = cur->next;
			continue;
		}

		id = xmlGetProp(cur, "id");
		printf("device %s\t", id);
		xmlFree(id);

		dev = cur->xmlChildrenNode;
		while (dev) {

			if (0 != xmlStrcmp(dev->name, (const xmlChar *)"name") &&
				0 != xmlStrcmp(dev->name, (const xmlChar *)"location") &&
				0 != xmlStrcmp(dev->name, (const xmlChar *)"type"))
			{
				dev = dev->next;
				continue;
			}

			key = xmlNodeGetContent(dev->xmlChildrenNode);
			if (key) {
				printf("%s: %s\t", dev->name, key);
				xmlFree(key);
			}

			dev = dev->next;
		}

		printf("\n");
		cur = cur->next;
	}

	xmlFreeDoc(doc);

	return 0;
}

int main(int argc, char *argv[])
{
	int ret;
	char *file = HSB_CONF_FILE;

	if (argc > 1)
		file = argv[1];

	if (0 != access(file, F_OK)) {
		printf("config file [%s] not exist\n", file);
		return -1;
	}

	if (ret = parse_hsb_config(file)) {
		printf("parse config file [%s] failed, ret=%d\n", file, ret);
		return -2;
	}

	return 0;
}
