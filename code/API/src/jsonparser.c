#ifndef _JSONPARSER_
#define _JSONPARSER_
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define JSON_DEBUG 0
#define MAX_KEYVAL_LENGTH 20
#define MAX_OBJ_PAIRS 10
#define MAX_OBJ_COUNT 20


typedef struct object json_object;
typedef struct pair key_val_pair;
typedef enum valType valueType;

enum valType
{
	boolean,
	string,
	integer
};

union value 
{
  char* string;
  char boolean;
  int integer;
  json_object* subObject;
};

struct pair
{
  char* key;
  union value val;
  valueType valT;
};

struct object
{
  key_val_pair* pairArray;
  int numPairs;
};


void printUsage(void);
json_object* parseJsonFile(FILE* file);
json_object* parseFileWithName(char* name);

char isObjBegin(char* line);
char isKeyValPair(char* line);
char isObjEnd(char* line);

char* getKey(char* line);
valueType getValueType(char* value);
char* getValueAsString(char* line);
void addKeyValPair(json_object* obj, char* line);

void debugPrintLn(char* line);
void printObj(const json_object* obj);

json_object* createJsonObj(ssize_t read, FILE* fp);

json_object* parseFileWithName(char* name)
{
  	FILE* fp;

  	//if(JSON_DEBUG) printf("\nFile path: %s\n", filePath);

  	fp = fopen(name, "r");

  	if(fp == 0)
    {
      	printf("Invalid path: could not open file\n");
      	return;
    }
  	else if(JSON_DEBUG)
    {
      	printf("File opened successfully\n\n");
    }

  	return parseJsonFile(fp);
}
void printUsage(void)
{
  	printf("Please provide the relative path to the json file you wish to parse\n");
}

json_object* parseJsonFile(FILE* fp)
{
	json_object* objectArr[MAX_OBJ_COUNT];
	int numObjs = 0;
  	char* line = NULL;
  	size_t len = 0;
  	ssize_t read;

  	while((read = getline(&line, &len, fp)) != -1)
    {     
      	debugPrintLn(line);

      	if(isObjBegin (line))
		{
	  	  	objectArr[numObjs] = createJsonObj (read, fp);
		 	numObjs++;
		}
      //shouldn't ever have key/value pairs here at the top level
	  //also means you can leave comments!
    }
  	fclose(fp);
  	return objectArr[0];
}

json_object* createJsonObj (ssize_t read, FILE* fp)
{
  	char* line = NULL;
  	size_t len = 0;

  	json_object* retObject = malloc (sizeof(json_object));
  	retObject->numPairs = 0;
  	retObject->pairArray = malloc(sizeof (key_val_pair) * MAX_OBJ_PAIRS);


  	if(JSON_DEBUG) printf("  --Allocated json_object--\n");

  	while((read = getline(&line, &len, fp)) != -1)
    {
      	debugPrintLn(line);

      	if(isObjEnd(line))
		{
	  		if(JSON_DEBUG) printf("  --Formal Obj End--\n");

	  		printObj(retObject);
	  		return retObject;
		}
      	else if(isKeyValPair(line))
		{
	  		addKeyValPair(retObject, line);
		}
    }
  	if(JSON_DEBUG) printf(" --Informal Obj End--\n");
  	return retObject;
}

void addKeyValPair(json_object* obj, char* line)
{
  	int num = obj->numPairs;
	char* tempStr;

  	obj->pairArray[num].key = getKey(line);

	tempStr = getValueAsString(line);
	obj->pairArray[num].valT = getValueType(tempStr);

	switch(obj->pairArray[num].valT)
	{
		case integer:
			obj->pairArray[num].val.integer = atoi(tempStr);
			break;
		case string:
			obj->pairArray[num].val.string = tempStr;
			break;
		case boolean:
			obj->pairArray[num].val.boolean = ((tempStr[0] == 't') ? 1 : 0);
			break;
	}
  	obj->numPairs++;
}

char isKeyValPair(char* line)
{
	int n = 0;
 
  	while(line[n] != '\n' && line[n] != '\0')
    {
      	if(line[n] == '"')
			return 1;

      	n++;
    }
  	return 0;
}

char* getKey(char* line)
{
  	char* key = malloc(MAX_KEYVAL_LENGTH * sizeof(char));
  	int lineN;
  	int keyN = 0;
  	char insideStr = 0;


  	for(lineN = 0; line[lineN] != '\n'; lineN++)
    {
      	if(line[lineN] != ' ')
		{
	  		if(line[lineN] == '"')
	    	{
	      		if(insideStr)
				{
		  			if(JSON_DEBUG) printf("  --extracted key/val: %s, ", key);

		  			return key;
				}
	      		else
					insideStr = 1;
	    	}
	  		else if(insideStr)
	    	{	
	      		key[keyN] = line[lineN];
	      		keyN++;
	    	}
		}
    }
}

valueType getValueType(char* value)
{
	if(value[0] == '"')	return string;
	else if(value[0] == 't' || value[0] == 'f') return boolean;
	else return integer;
}

char* getValueAsString(char* line)
{
  	char* value = malloc(MAX_KEYVAL_LENGTH * sizeof(char));

  	int lineN = 0;
  	int valueN = 0;
  	char insideStr = 0;

  	while(line[lineN] != ':')
    	lineN++;

  	lineN++;

  	while(line[lineN] == ' ')
    	lineN++;

  	while(line[lineN] != ',' && line[lineN] != '\n' && line[lineN] != ' ')
    {
      	value[valueN] = line[lineN];
      	lineN++;
      	valueN++;
    }
  	if(JSON_DEBUG) printf("%s--\n", value);
  	return value;
}

char isObjBegin(char* line)
{
  	return ( (line[0] == '{') ? 1 : 0);
}
char isObjEnd(char* line)
{
  	return ( (line[0] == '}') ? 1 : 0);
}

void debugPrintLn(char* line)
{
  	if(JSON_DEBUG)
    {
      	if(line[0] != '\n')
			printf("%s", line);
    }
}

void printObj(const json_object* obj)
{
 	int n;
	char* printString;
	key_val_pair tempPair;

	printf("\n   -- New Object --\n");
  	for(n = 0; n < obj->numPairs; n++)
    {
		tempPair = obj->pairArray[n];

		if(tempPair.valT == string)
			printString = tempPair.val.string;

		else if(tempPair.valT == boolean)
		{
			if(tempPair.val.boolean == 1)
				printString = "true";
			else
				printString = "false";
		}
		else if(tempPair.valT == integer)
			sprintf(printString, "%d", tempPair.val.integer);

      	printf("Key: '%s', Val: '%s'\n", tempPair.key, printString);
		
    }
}
#endif
