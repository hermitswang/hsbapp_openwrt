
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include <libxml/xmlmemory.h>
#include <libxml/parser.h>
#include <libxml/tree.h>

#define HSB_CONF_FILE	"hsb.xml"

static int create_hsb_config(char *file)
{
	xmlDocPtr doc = NULL;
	xmlNodePtr root = NULL;
	xmlNodePtr node = NULL;
	xmlNodePtr prty = NULL;
	xmlNodePtr val = NULL;

	doc = xmlNewDoc(BAD_CAST"1.0");

	root = xmlNewNode(NULL, BAD_CAST"root");
	xmlDocSetRootElement(doc, root);

	node = xmlNewNode(NULL, BAD_CAST"device");

	prty = xmlNewNode(NULL, BAD_CAST"type");
	val = xmlNewText(BAD_CAST"2");
	xmlAddChild(prty, val);
	xmlAddChild(node, prty);

	prty = xmlNewNode(NULL, BAD_CAST"name");
	val = xmlNewText(BAD_CAST"remote");
	xmlAddChild(prty, val);
	xmlAddChild(node, prty);

	prty = xmlNewNode(NULL, BAD_CAST"location");
	val = xmlNewText(BAD_CAST"客厅");
	xmlAddChild(prty, val);
	xmlAddChild(node, prty);

	xmlAddChild(root, node);

	xmlSaveFormatFileEnc(file, doc, "UTF-8", 1);

	xmlFreeDoc(doc);

	return 0;
}

int main(int argc, char *argv[])
{
	int ret;
	char *file = HSB_CONF_FILE;

	if (argc > 1)
		file = argv[1];

	if (ret = create_hsb_config(file)) {
		printf("create config failed, ret=%d\n", ret);
		return ret;
	}

	return 0;
}
