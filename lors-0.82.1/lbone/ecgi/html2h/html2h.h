#define KindParam	1
#define KindSLink	2

typedef struct _Input{
	char *type;
	char *name;
	struct _Input *next;
}Input;

typedef struct _Content{
	int kind;
	char *name;
	char *comment;
	char *format;
	struct _Content *next;
	struct _Content *sub;
}Content;

typedef struct _Section{
	char *name;
	char *comment;
	Content *list;
	Input *ilist;
	struct _Section *next;
}Section;

void fileWrite(MFILE *mf, char *fname);
MFILE *fileRead(char *fname);
void searchInput(const char *flastok, const char *fhit);
void parseInput(const char *input);
char *parseInputValue(const char *str, const char *name);
void parseMeta(MFILE *mfout, const char *toparse);
void parseSection(MFILE *mfout, const char *params);
char *parseParam(MFILE *mfout, Content *insat, const char *params, int printpname);
void parseSLink(MFILE *mfout, const char *params);

Section *listInsert(const char *name, const char *comment);
Content *listInsertContent(Content *where, const char *format, 
		const char *name, const char *comment);
void listInsertInput(const char *name, const char *type);
void listDump(FILE *f);

void escapeWrite(char *what, int length, MFILE *where);
void pexit(char *msg, const char *comment);

int commdepth=0;
Section *secakt=NULL, *seclist=NULL;