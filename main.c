#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h>
#include <windows.h>
#include <io.h>
#include <direct.h>
#include <time.h>

#define Command_Mode 0
#define Replace_Mode 1
#define Insert_Mode 2
#define startCol_message_coord 58
#define startCol_message_EditMode 70
#define endCol_per_line 79
#define endLine_per_page 21
#define command_line 22
#define message_line 23
#define error_message_line 24
#define Marked_Block 1
#define Marked_Line 2
#define Marked_Char 3

int openFile(int argc, char * fileName) ;
void initConsole(int fileExist) ;
void cleanFile() ;
void cleanFileName() ;
void cleanContent() ;
void cleanCommand() ;
void cleanMarkPos() ;
void setFileName(char * fileName) ;
void inputChar(char data) ;
void findString(char * data) ;
void deleteMarkedText() ;
void doCurrentCommand();
void markedString(COORD startPos, COORD endPos);


typedef struct Content {
    char * data ;
    struct Content * next ;
}Content;

typedef struct FileData {
	char * g_fileName;
	struct Content * g_content;
	struct FileData * pre;
	struct FileData * next;
}FileData;

struct FileData * g_currentFile = NULL;

char * g_command = NULL;
COORD g_file_pos, g_cursor_pos, g_window_pos, g_command_pos, g_markStart_pos, g_markEnd_pos ;    // File position, cursor position, window position and command position
int g_Current_Mode = Command_Mode, g_Edit_Mode = Insert_Mode, g_Marked_Mode = 0 ;   // Current mode, Edit mode and Marked mode
int g_errorFlag = 0, g_findStringFlag = 0, g_ReplaceMarkedEnableFlag = 0, g_initReplaceMarkedFlag = 0, g_waitAnsFlag = 0, g_getAnsFlag = 0;

void eraseErrMsg()
{
	HANDLE hout = GetStdHandle(STD_OUTPUT_HANDLE) ;
	DWORD dwTT ;
	COORD temp ;

	temp.X = 0 ;
	temp.Y = error_message_line ;

	FillConsoleOutputCharacter(hout, ' ', (endCol_per_line + 1), temp, &dwTT) ;
}

void perr(char *msg)
{
	HANDLE hout = GetStdHandle(STD_OUTPUT_HANDLE) ;
	COORD temp ;

	temp.X = 0 ;
	temp.Y = error_message_line ;

	eraseErrMsg() ;

	SetConsoleCursorPosition(hout, temp) ;
	printf("%s", msg) ;

	g_errorFlag = 1 ;
}

Content * newContent()
{
	Content * newData = (struct Content*)malloc(sizeof(struct Content)) ;

	memset(newData, 0x00, sizeof(struct Content));

	if(!newData)
		perr("New Content failed!") ;
	else
	{
		newData->data = NULL ;
		newData->next = NULL ;
	}

	return newData ;
}

FileData * newFileData()
{
	FileData * newFile = (struct FileData*)malloc(sizeof(struct FileData));

	memset(newFile, 0x00, sizeof(struct FileData));

	if (!newFile)
		perr("New file failed!");
	else
	{
		newFile->g_content = newContent();
		newFile->g_fileName = NULL;
		newFile->next = NULL;
		newFile->pre = NULL;
	}

	return newFile;
}

/* Add new file next to current file, and move current ptr to next */
void addNewFile()
{
	FileData * tempFile = NULL;

	tempFile = newFileData();

	// Add new fileData
	if (!g_currentFile)
		g_currentFile = tempFile;
	else
	{
		// Add fileData after current file
		if (g_currentFile->next == NULL)
			tempFile->next = g_currentFile;
		else
		{
			g_currentFile->next->pre = tempFile;
			tempFile->next = g_currentFile->next;
		}

		g_currentFile->next = tempFile;

		if (g_currentFile->pre == NULL)
			g_currentFile->pre = tempFile;
		
		tempFile->pre = g_currentFile;

		// Move current file data to next
		g_currentFile = g_currentFile->next;
	}
}

int isValidCoord()
{
	int isValid = 1 ;

	if((g_Current_Mode == Command_Mode) && ((g_command_pos.X > endCol_per_line) || (g_command_pos.Y != command_line)))
	{
		perr("Invalid command coordinate!") ;
		isValid = 0 ;
	}
	else if((g_file_pos.X < 0) || (g_file_pos.Y < 0) || (g_cursor_pos.X < 0) || (g_cursor_pos.Y < 0) || (g_cursor_pos.X > endCol_per_line) || (g_cursor_pos.Y > endLine_per_page))
	{
		perr("Invalid data coordinate!") ;
		isValid = 0 ;
	}

	return isValid ;
}

void shiftL()
{
	if(isValidCoord())
	{
		if((g_Current_Mode == Command_Mode) && (g_command_pos.X > 0))
		{
			g_command_pos.X -= 1 ;
		}
		else if((g_Current_Mode != Command_Mode) && (g_file_pos.X > 0))
		{
			g_file_pos.X -= 1 ;
			
			if(g_cursor_pos.X > 0)
				g_cursor_pos.X -= 1 ;
		}
	}
}

void shiftR()
{
	if(isValidCoord())
	{
		if((g_Current_Mode == Command_Mode) && (g_command != NULL) && ((int)strlen(g_command) > g_command_pos.X))
		{
			g_command_pos.X += 1 ;
		}
		else if(g_Current_Mode != Command_Mode)
		{
			g_file_pos.X += 1 ;

			if(g_cursor_pos.X < endCol_per_line)
				g_cursor_pos.X += 1 ;
		}
	}
}

void shiftUp()
{
	if(isValidCoord() && g_file_pos.Y > 0)
	{
		g_file_pos.Y -= 1 ;

		if(g_cursor_pos.Y > 0)
			g_cursor_pos.Y -= 1 ;
	}
}

void shiftDn()
{
	if(isValidCoord())
	{
		g_file_pos.Y += 1 ;

		if(g_cursor_pos.Y < endLine_per_page)
			g_cursor_pos.Y += 1 ;
	}
}

void home()
{
	if(isValidCoord())
	{
		g_file_pos.X = 0 ;
		g_cursor_pos.X = 0 ;
	}
}

void end()
{
	int i = 0, j = 0 ;
	Content * ptr = g_currentFile->g_content;
	
	if(isValidCoord())
	{
		for(i = 0 ; i <= g_file_pos.Y ; i++, ptr = ptr->next)
		{
			if(!ptr)
			{
				g_file_pos.X = 0 ;
				g_cursor_pos.X = 0 ;
				break ;
			}

			if(i == g_file_pos.Y)
			{
				if(ptr->data)
				{
					g_file_pos.X = strlen(ptr->data) - 1 ;

					if(g_file_pos.X <= endCol_per_line)
						g_cursor_pos.X = g_file_pos.X ;
					else
						g_cursor_pos.X = (endCol_per_line / 2) ;
				}
				else
				{
					g_file_pos.X = 0 ;
					g_cursor_pos.X = 0 ;
				}
			}
		}
	}
}

void prior()
{
	if(isValidCoord())
	{
		if(g_file_pos.Y >= (endLine_per_page + 1))
			g_file_pos.Y -= (endLine_per_page + 1) ;
		else
		{
			g_file_pos.Y = 0 ;
			g_cursor_pos.Y = 0 ;
		}
	}
}

void next()
{
	if(isValidCoord())
		g_file_pos.Y += (endLine_per_page + 1) ;
}

void ctrl_left()
{
	if(isValidCoord())
	{
		if(g_file_pos.X > (endCol_per_line / 2))
			g_file_pos.X -= ((endCol_per_line / 2) + (endCol_per_line % 2)) ;
		else
			g_file_pos.X = 0 ;

		if(g_cursor_pos.X > (endCol_per_line / 2))
			g_cursor_pos.X -= ((endCol_per_line / 2) + (endCol_per_line % 2)) ;
		else
			g_cursor_pos.X = 0 ;
	}
}

void ctrl_right()
{
	if(isValidCoord())
	{
		g_file_pos.X += ((endCol_per_line / 2) + (endCol_per_line % 2)) ;

		if(g_cursor_pos.X <= (endCol_per_line / 2))
			g_cursor_pos.X += ((endCol_per_line / 2) + (endCol_per_line % 2)) ;
	}
}

void ctrl_home()
{
	if(isValidCoord())
	{
		g_file_pos.Y = 0 ;
		g_cursor_pos.Y = 0 ;
	}
}

void ctrl_end()
{
	int i = 0 ;
	Content * ptr = g_currentFile->g_content;

	if(isValidCoord())
	{
		for(i = 0 ;  ; i++, ptr = ptr->next)
		{
			if(!ptr)
			{
				if(i == 0)
				{
					g_file_pos.Y = 0 ;
					g_cursor_pos.Y = 0 ;
				}
				else
				{
					g_file_pos.Y = i - 1 ;

					if((i - 1) > endLine_per_page)
						g_cursor_pos.Y = endLine_per_page ;
					else
						g_cursor_pos.Y = i - 1 ;
				}

				break ;
			}
		}
	}
}

void ctrl_prior()
{
	if(isValidCoord())
	{
		g_file_pos.Y -= g_cursor_pos.Y ;
		
		if(g_file_pos.Y < 0)
			g_file_pos.Y = 0 ;

		g_cursor_pos.Y = 0 ;
	}
}

/* Delete one character on current position */
void delete_char()
{
	int i = 0, newData_size = 0, moveData_size = 0 ;
	Content * ptr = g_currentFile->g_content, *pre_ptr = NULL, *temp = NULL;
	
	if(isValidCoord())
	{
		if((g_Current_Mode == Command_Mode) && (g_command) && (g_command_pos.X < (int)strlen(g_command)))     // In Command mode...
		{
			newData_size = (strlen(g_command) + 1) - 1 ;
			moveData_size = (strlen(g_command) + 1) - (g_command_pos.X + 1) ; 

			memmove(&(g_command[g_command_pos.X]), &(g_command[g_command_pos.X + 1]), moveData_size) ;
			g_command = (char*)realloc(g_command, sizeof(char) * newData_size) ;
		}
		else if(g_Current_Mode != Command_Mode)                                                               // In Edit mode...
		{
			for(i = 0 ; i <= g_file_pos.Y ; i++, ptr = ptr->next)
			{
				if(!ptr)
					break ;
				// Find the corresponding line
				if(i == g_file_pos.Y)
				{
					if((!(ptr->data) && (g_file_pos.X == 0)))                                            // If current content is empty, delete current line
					{
						if(pre_ptr)
							pre_ptr->next = ptr->next ;
						else
							g_currentFile->g_content = ptr->next;

						free(ptr) ;
					}
					else if((ptr->data) && ((unsigned int)(g_file_pos.X) == (strlen(ptr->data) - 1)) && (ptr->next))    // If at the end of current content, move data from next line to end of line
					{
						temp = ptr->next ;

						if(ptr->next->data)
						{
							newData_size = (strlen(ptr->data) + 1) + (strlen(ptr->next->data) - 1) ;

							ptr->data = (char*)realloc(ptr->data, (sizeof(char) * newData_size)) ;
							memcpy(&(ptr->data[g_file_pos.X]), ptr->next->data, (strlen(ptr->next->data) + 1)) ;

							free((ptr->next->data)) ;
							ptr->next->data = NULL ;
						}

						ptr->next = ptr->next->next ;

						free(temp) ;
					}
					else if((ptr->data) && ((unsigned int)(g_file_pos.X) < (strlen(ptr->data) - 1)))     // If in the middle of current content, shit left all data after current position
					{
						newData_size = (strlen(ptr->data) + 1) - 1 ;
						moveData_size = (strlen(ptr->data) + 1) - (g_file_pos.X + 1) ; 

						memmove(&(ptr->data[g_file_pos.X]), &(ptr->data[g_file_pos.X + 1]), moveData_size) ;
						ptr->data = (char*)realloc(ptr->data, sizeof(char) * newData_size) ;
					}

					break ;
				}

				pre_ptr = ptr ;
			}
		}
	}
}

void backspace()
{
	int shouldDelete = 1 ;
	int temp_pos_X = g_file_pos.X, temp_cursor_X = g_cursor_pos.X ;

	if(isValidCoord())
	{
		if((g_Current_Mode == Command_Mode) && (g_command_pos.X > 0))
		{
			shiftL() ;
			delete_char() ;
		}
		if((g_Current_Mode != Command_Mode) && !((g_file_pos.X == 0) && (g_file_pos.Y == 0)))
		{
			// Check if content should be deleted
			end() ;

			if(g_file_pos.X < temp_pos_X)
				shouldDelete = 0 ;

			g_file_pos.X = temp_pos_X ;
			g_cursor_pos.X = temp_cursor_X ;

			// Move cursor
			if(g_file_pos.X > 0)
				shiftL() ;
			else if((g_file_pos.X == 0) && (g_file_pos.Y > 0))
			{
				shiftUp() ;
				end() ;
			}

			// delete one character
			if(shouldDelete)
				delete_char() ;
		}
	}
}

/* Delete content to end of line                                    */
/* isEraceAll :  1 -> erace all ; 0 -> erace from current position  */
void erase(int isEraseAll)
{
	int i = 0 ;
	Content * ptr = g_currentFile->g_content;

	if(isValidCoord())
	{
		for(i = 0 ; i <= g_file_pos.Y ; i++, ptr = ptr->next)
		{
			if (!ptr)
				break;

			// Find the corresponding line
			if(i == g_file_pos.Y)
			{
				// Remove data : all / after cursor position
				if((ptr->data) && (isEraseAll || g_file_pos.X == 0))
				{
					free(ptr->data) ;
					ptr->data = NULL ;
				}
				else if((ptr->data) && ((unsigned int)(g_file_pos.X) < strlen(ptr->data)))
				{
					ptr->data = (char*)realloc(ptr->data, (sizeof(char) * (g_file_pos.X + 2))) ;
					ptr->data[g_file_pos.X] = '\n' ;
					ptr->data[g_file_pos.X + 1] = '\0' ;
				}

				break ;
			}
		}
	}
}

int popToken(char *token, char *buf, int tokenSize)
{
	char * temp = NULL, *p = NULL ;

	memset(token, 0x00, tokenSize);

	if(buf && (temp = strtok_s(buf, " ", &p)))
	{
		strcpy_s(token, tokenSize, temp);
		token[strlen(temp)] = '\0' ;

		if((temp = strtok_s(NULL, " ", &p)))
		{
			char buf_temp[100] ;

			memset(buf_temp, 0x00, sizeof(char) * 100) ;

			do
			{
				strcat_s(buf_temp, (sizeof(char) * 100), temp);
				strcat_s(buf_temp, (sizeof(char) * 100), " ");
			}
			while((temp = strtok_s(NULL, " ", &p))) ;
			
			buf_temp[strlen(buf_temp) - 1] = '\0' ;
			strcpy_s(buf, (sizeof(char)) * (strlen(g_command) + 1), buf_temp);
		}
		else
		{
			if (buf)
				memset(buf, 0x00, sizeof(char) * strlen(buf)) ;
		}
	}

	return strlen(token);
}

char * combineDirtoFileName(char * fileDir, char * fileName)
{
	char * newFileName = NULL;
	int newFileNameSize = 0, i = 0;
	
	if (fileDir && fileName)
	{
		for (i = strlen(fileName) - 1; i >= 0; i--)
			if (fileName[i] == '\\')
				break;

		newFileNameSize = strlen(fileDir) + (strlen(fileName) - (i + 1)) + 2;

		newFileName = (char *)malloc(sizeof(char) * newFileNameSize);
		memset(newFileName, 0x00, sizeof(char) * newFileNameSize);
		strcpy_s(newFileName, newFileNameSize, fileDir);
		newFileName[strlen(fileDir)] = '\\';
		memcpy(&(newFileName[strlen(fileDir) + 1]), &(fileName[i + 1]), newFileNameSize - strlen(fileDir));
	}

	return newFileName;
}

void changeFileDirectory(char * newfileDir)
{
	char currentFileDir[MAX_PATH] = {0}, *newFileName = NULL;
	int newFileNameSize = 0, i = 0;

	GetCurrentDirectory(MAX_PATH, currentFileDir);

	if (SetCurrentDirectory(newfileDir) == 0)
		perr("This directory cannot be accessed!");
	else
	{
		SetCurrentDirectory(currentFileDir);

		newFileName = combineDirtoFileName(newfileDir, g_currentFile->g_fileName);

		if (newFileName != NULL)
			setFileName(newFileName);
	}
}

/* Check if newFileName is different from all open file in g_currentFile */
int checkIsNewFileName(char * newFileName, FileData ** existFile)
{
	int isNewFile = 1;
	FileData * ptr = g_currentFile;

	if (newFileName)
	{
		do
		{
			if ((ptr->g_fileName) && (strcmp(newFileName, ptr->g_fileName) == 0))
			{
				isNewFile = 0;
				break;
			}
			ptr = ptr->next;

		} while ((ptr) && (ptr != g_currentFile));
	}
	else
		isNewFile = 0;

	if (isNewFile || !newFileName)
		*existFile = NULL;
	else
		*existFile = ptr;

	return isNewFile;
}

/* Set directory if file name without it, return original if already had it */
char * setCurrentDirtoFileName(char * fileName)
{
	char currentFileDir[MAX_PATH] = { 0 }, *tempFileName = fileName, *p = NULL;
	int i = 0;

	if (fileName)
	{
		for (i = 0; i < (int)strlen(fileName); i++)
		{
			if (fileName[i] == '\\')
				break;
			else if (i == (int)strlen(fileName) - 1)
			{
				GetCurrentDirectory(MAX_PATH, currentFileDir);
				tempFileName = combineDirtoFileName(currentFileDir, fileName);
			}
		}
	}

	return tempFileName;
}

void saveFileWitheOtherName(char * com)
{
	char token[100] = { 0 }, * tempFileName = NULL;
	int len = 0 ;
	FileData * existFile = NULL;

	len = popToken(token, com, 100) ;

	// If got new name
	if (len > 0)
	{
		tempFileName = setCurrentDirtoFileName(token);

		// Check if the name is different from all open and exist files
		if ((checkIsNewFileName(tempFileName, &existFile) == 1) && (_access(tempFileName, 0) != 0))
		{
			// Reset file name
			cleanFileName();
			setFileName(tempFileName);
		}
		else
			perr("Duplicate name with other file!");
	}
}

void editOtherFile(char * com)
{
	char token[100] = { 0 }, * tempFileName = NULL;
	int len = 0, fileExist = 0 ;
	FileData * existFile = NULL;

	len = popToken(token, com, 100) ;

	if ((len == 0) && (g_currentFile->next))
		g_currentFile = g_currentFile->next;
	else if (len > 0)
	{
		tempFileName = setCurrentDirtoFileName(token);

		// Check if the name is different from all open files
		if (checkIsNewFileName(tempFileName, &existFile) == 1)
		{
			fileExist = openFile(2, tempFileName);
			initConsole(fileExist);
		}
		else if (existFile)
		{
			g_currentFile = existFile;
			perr("File exist.");
		}
	}
}

void writeLineintoContent(Content * ptr, char * str)
{
	if (ptr && str)
	{
		ptr->data = (char*)realloc(ptr->data, sizeof(char) * (strlen(str) + 1));
		memset(ptr->data, 0x00, sizeof(char) * (strlen(str) + 1));
		memcpy(ptr->data, str, sizeof(char) * strlen(str));

		if (ptr->data[strlen(str) - 1] == '\n')
			ptr->data[strlen(str)] = '\0';
		else
		{
			ptr->data = (char*)realloc(ptr->data, sizeof(char) * (strlen(str) + 2));
			ptr->data[strlen(str)] = '\n';
			ptr->data[strlen(str) + 1] = '\0';
		}
	}
}

Content * writeDirListInitintoFileData(char * dir)
{
	Content * ptr = g_currentFile->g_content;
	int tempStringSize = 0;
	char *tempString = NULL;

	if (dir)
	{
		tempStringSize = 4 + strlen(dir) + 1;
		tempString = (char *)realloc(tempString, sizeof(char) * tempStringSize);
		sprintf_s(tempString, tempStringSize, "%s%s", "dir ", dir);
		writeLineintoContent(ptr, tempString);

		if (!ptr->next)
			ptr->next = newContent();
		ptr = ptr->next;

		writeLineintoContent(ptr, "\n");

		if (!ptr->next)
			ptr->next = newContent();
		ptr = ptr->next;

		tempStringSize = 25 + 14 + strlen("Last Edit Date") + 1;
		tempString = (char *)realloc(tempString, sizeof(char) * tempStringSize);
		sprintf_s(tempString, tempStringSize, "%-25s%-14s%-s", "File Name", "File Size", "Last edit Date");
		writeLineintoContent(ptr, tempString);

		if (!ptr->next)
			ptr->next = newContent();
		ptr = ptr->next;

		writeLineintoContent(ptr, "\n");

		if (!ptr->next)
			ptr->next = newContent();
		ptr = ptr->next;
	}

	if (tempString)
		free(tempString);

	return ptr;
}

void writeDirListintoFileData(char * dir)
{
	long handle;
	struct _finddata_t fileinfo;
	Content * ptr = g_currentFile->g_content, * ptr_end = NULL;
	int tempStringSize = 0;
	char tempFileName[24] = {0}, tempTime[26] = { 0 }, *tempString = NULL;

	handle = _findfirst(dir, &fileinfo);

	ptr = writeDirListInitintoFileData(dir);

	if (handle != -1)
	{
		do
		{
			if (strlen(fileinfo.name) >= 24)
			{
				memcpy(tempFileName, fileinfo.name, 22);
				tempFileName[22] = '~';
				tempFileName[23] = '\0';
			}
			else
				strcpy_s(tempFileName, 24, fileinfo.name);

			ctime_s(tempTime, 26, &fileinfo.time_write);
			tempStringSize = 25 + 14 + 1 + strlen(tempTime);

			tempString = (char *)realloc(tempString, sizeof(char) * tempStringSize);
			memset(tempString, 0x00, sizeof(char) * tempStringSize);

			// Check if it is a DIR
			if (fileinfo.attrib & _A_SUBDIR)
				sprintf_s(tempString, tempStringSize, "%-25s%-14s%-s", tempFileName, "<< DIR >> ", tempTime);
			else
				sprintf_s(tempString, tempStringSize, "%-25s%-14d%-s", tempFileName, fileinfo.size, tempTime);
			
			writeLineintoContent(ptr, tempString);

			if (!ptr->next)
				ptr->next = newContent();
			ptr_end = ptr;
			ptr = ptr->next;
		}while (!_findnext(handle, &fileinfo));

		ptr_end->next = NULL;
		free(ptr);

		_findclose(handle);
	}
	else
		perr("No file in this directory!");

	if (tempString)
		free(tempString);
}

void listDirectory(char * dir)
{
	char currentFileDir[MAX_PATH] = { 0 }, *tempDir = NULL;
	char dirFileName[8] = "dir.txt\0";

	GetCurrentDirectory(MAX_PATH, currentFileDir);

	if (SetCurrentDirectory(dir) == 0)
		perr("This directory cannot be accessed!");
	else
	{
		SetCurrentDirectory(currentFileDir);

		tempDir = (char *)malloc((sizeof(char) * (strlen(dir) + 5)));
		memset(tempDir, 0x00, (strlen(dir) + 3 + 2));

		sprintf_s(tempDir, (strlen(dir) + 5), "%s%s", dir, "\\*.*");

		editOtherFile(dirFileName);
		cleanContent();
		g_currentFile->g_content = newContent();

		writeDirListintoFileData(tempDir);
	}

	if (tempDir)
		free(tempDir);
}

/* Use newString to replace Marked string */
void replaceMarkedString(char * newString)
{
	int tempMode = g_Edit_Mode, i = 0;
	char *p = NULL;
	COORD start, end;
	
	// Check if command is valid => /string/new string/
	if (newString && g_findStringFlag && g_ReplaceMarkedEnableFlag)
	{
		// Delete oringinal text
		deleteMarkedText();
		start.X = g_file_pos.X;
		start.Y = g_file_pos.Y;

		// Input new string
		g_Edit_Mode = Insert_Mode;
		
		for (i = 0; i < (int)strlen(newString); i++)
			inputChar(newString[i]);

		g_Edit_Mode = tempMode;

		// Remarked new string
		end.X = g_file_pos.X - 1;
		end.Y = g_file_pos.Y;
		markedString(start, end);
	}
}

int checkIsValidReplaceMarkedStr(char * command, char ** string, char ** newString)
{
	int isValidCommand = 0, tempMode = g_Edit_Mode, i = 0;
	char *p = NULL;

	// Check if command is valid => /string/new string/
	if ((command != NULL) && (strlen(command) >= 5) && (command[0] == '/') && ((command[strlen(command) - 1] == '/')))
	{
		*string = strtok_s(command, "/", &p);
		*newString = strtok_s(NULL, "/", &p);

		if (*string && *newString)
			isValidCommand = 1;
	}

	return isValidCommand;
}

/* Use newString to find and replace original string */
void replaceStrbyNStr(char * string, char * newString)
{
	if (g_initReplaceMarkedFlag)    // For initial
	{
		perr("Pattern has been set.");
		g_initReplaceMarkedFlag = 0;
		g_ReplaceMarkedEnableFlag = 1;
	}
	else                            // Find and replace original string
	{
		// Find and marked string
		if (!g_getAnsFlag)
			findString(string);

		// Use newString to replace marked string
		if (g_findStringFlag && g_ReplaceMarkedEnableFlag && !g_waitAnsFlag)
			replaceMarkedString(newString);
	}
}

void doCurrentCommand()
{
	char token[100] = {0}, tempString[100] = {0};
	char * string = NULL, * newString = NULL;
	char * temp = (char*)malloc((sizeof(char)) * (strlen(g_command) + 1)) ;
	int len = 0, ch = 0, isValid = 0 ;

	memset(temp, 0x00, sizeof(char) * (strlen(g_command) + 1)) ;
	memcpy(temp, g_command, (strlen(g_command) + 1)) ;

	len = strlen(temp);

	if(len > 0)
	{
		len = popToken(token, temp, (sizeof(char) * (strlen(g_command) + 1)));

		if(len == 1)
		{
			ch = tolower(token[0]) ;
			isValid = 1 ;

			switch(ch)
			{
				case 'e':    // Edit new or exist file
					editOtherFile(temp) ;
					break;
				case 'n':    // Assign file name for current edit
					saveFileWitheOtherName(temp) ;
					break;
				case 'c' :   // Replace string by new string
					popToken(token, temp, (sizeof(char) * (strlen(g_command) + 1)));
					isValid = checkIsValidReplaceMarkedStr(token, &string, &newString);
					if (isValid)
						replaceStrbyNStr(string, newString);
					break;
				default:
					isValid = 0 ;
					break;
			}
		}
		else if ((len > 1) && (token[0] == '/'))   // Find string
		{
			memcpy(tempString, &token[1], (sizeof(char)) * (len - 1));
			findString(tempString);
			isValid = 1;
		}
		else if ((len == 2) && (tolower(token[0]) == 'c') && (tolower(token[1]) == 'd'))   // Change current directory
		{
			popToken(token, temp, (sizeof(char) * (strlen(g_command) + 1)));
			changeFileDirectory(token);
			isValid = 1;
		}
		else if ((len == 3) && (tolower(token[0]) == 'd') && (tolower(token[1]) == 'i') && (tolower(token[2]) == 'r'))  // Show directory list
		{
			popToken(token, temp, (sizeof(char) * (strlen(g_command) + 1)));
			listDirectory(token);
			isValid = 1;
		}
	}

	if(isValid == 0)
		perr("Invalid command.") ;

	if (temp)
		free(temp) ;
}

void enter()
{
	int i = 0, newData_size = 0 ;
	Content * ptr = g_currentFile->g_content, *newData = NULL;

	if(isValidCoord())
	{
		if((g_Current_Mode == Command_Mode) && (g_command))
		{
			// Initial finding string flag and replace marked string flag
			g_findStringFlag = 0;
			g_ReplaceMarkedEnableFlag = 0;
			g_initReplaceMarkedFlag = 1;

			doCurrentCommand() ;
		}
		else if(g_Current_Mode != Command_Mode)
		{
			for(i = 0 ; i <= g_file_pos.Y ; i++, ptr = ptr->next)
			{
				if(!ptr)
				{
					inputChar('\n') ;
					break ;
				}
				// Find the corresponding line
				if(i == g_file_pos.Y)
				{
					// Create new line
					newData = (struct Content*)malloc(sizeof(struct Content)) ;
					newData->data = NULL ;
					newData->next = ptr->next ;
					ptr->next = newData ;

					// Copy data from current position
					if((ptr->data) && ((unsigned int)(g_file_pos.X) < (strlen(ptr->data) - 1)))
					{
						newData_size = strlen(ptr->data) - (g_file_pos.X + 1) + 2 ;

						newData->data = (char*)malloc(sizeof(char) * newData_size) ;
						memset(newData->data, 0x00, (sizeof(char) * newData_size)) ;
						memcpy(newData->data, &(ptr->data[g_file_pos.X]), newData_size) ;
					}

					break ;
				}
			}

			erase(0) ;
			shiftDn() ;
			home() ;
		}
	}
}

void setMarkPos(char mode)
{
	if(isValidCoord())
	{
		// Setting mode
		switch (mode)
		{
			case 'b':
				g_Marked_Mode = Marked_Block;
				break;
			case 'l' :
				g_Marked_Mode = Marked_Line;
				break;
			case 'c' :
				g_Marked_Mode = Marked_Char;
				break;
			default :
				break;
		}

		// Setting marked coordinate
		if((g_markStart_pos.X > -1) && (g_markStart_pos.Y > -1))
		{
			g_markEnd_pos.X = g_file_pos.X ;
			g_markEnd_pos.Y = g_file_pos.Y ;
		}
		else
		{
			g_markStart_pos.X = g_file_pos.X ;
			g_markStart_pos.Y = g_file_pos.Y ;
		}
	}
}

/* Input one character     */
/* data : input char       */
void inputChar(char data)
{
	int i = 0, moveData_size = 0, newData_size = 0 ;
	Content * ptr = g_currentFile->g_content;

	if(isValidCoord())
	{
		if(g_Current_Mode == Command_Mode)
		{
			// Initial finding string flag and replace marked string flag
			g_findStringFlag = 0;
			g_ReplaceMarkedEnableFlag = 0;

			// Calculate command size
			if((!g_command) || (g_Edit_Mode == Insert_Mode) || (g_command_pos.X == (int)strlen(g_command)))
			{
				if(!g_command)
					newData_size = 2 ;
				else
				{
					newData_size = strlen(g_command) + 2 ;
					moveData_size = (strlen(g_command) + 1) - (g_command_pos.X + 1) + 1 ;
				}

				g_command = (char*)realloc(g_command, (sizeof(char) * newData_size)) ;

				if(moveData_size != 0)
					memmove(&(g_command[g_command_pos.X + 1]), &(g_command[g_command_pos.X]), moveData_size) ;
				else
					g_command[newData_size - 1] = '\0' ;
			}

			// Write data in g_command
			g_command[g_command_pos.X] = data ;
			
			// If in Insert mode, shift cursor right
			if(g_Edit_Mode == Insert_Mode)
				shiftR() ;
		}
		else
		{
			for(i = 0 ; i <= g_file_pos.Y ; i++, ptr = ptr->next)
			{
				// Find the corresponding line
				if(i == g_file_pos.Y)
				{
					// Write data in current position
					if((ptr->data) && ((unsigned int)(g_file_pos.X) < (strlen(ptr->data) - 1)))     // In the middle of current content : Replace/Insert mode
					{
						// Check if in Insert mode
						if(g_Edit_Mode == Insert_Mode)
						{
							moveData_size = (strlen(ptr->data) + 1) - (g_file_pos.X + 1) + 1 ;
							newData_size = strlen(ptr->data) + 2 ;

							ptr->data = (char*)realloc(ptr->data, (sizeof(char) * newData_size)) ;
							memmove(&(ptr->data[g_file_pos.X + 1]), &(ptr->data[g_file_pos.X]), moveData_size) ;
						}

						// Write data
						ptr->data[g_file_pos.X] = data ;
					}
					else
					{
						int j = 0 ;

						// Check if data exist
						if(ptr->data)
							j = strlen(ptr->data) - 1 ;
						
						if(data == '\n')
							ptr->data = (char*)realloc(ptr->data, (sizeof(char) * (g_file_pos.X + 2))) ;
						else
							ptr->data = (char*)realloc(ptr->data, (sizeof(char) * (g_file_pos.X + 3))) ;
						
						// Write data
						for( ; j <= g_file_pos.X ; j++)
						{
							if(j == g_file_pos.X)
								ptr->data[j] = data ;			
							else
								ptr->data[j] = ' ' ;
						}

						if(data == '\n')
							ptr->data[g_file_pos.X + 1] = '\0' ;
						else
						{
							ptr->data[g_file_pos.X + 1] = '\n' ;
							ptr->data[g_file_pos.X + 2] = '\0' ;
						}
					}

					// If in Insert mode, shift cursor right
					if(g_Edit_Mode == Insert_Mode)
						shiftR() ;

					break ;
				}

				// If end of file but still cannot find the corresponding line, create a new line
				if(!(ptr->next))
				{
					ptr->next = newContent() ;
				}
			}
		}
	}
}

void normalMarkedBlockPos(COORD *tempStart, COORD *tempEnd)
{
	if ((g_markStart_pos.X != -1) && (g_markStart_pos.Y != -1))
	{
		// Normalization the marked coordinate
		if ((g_markEnd_pos.X != -1) && (g_markEnd_pos.Y != -1))
		{
			if (g_markStart_pos.X < g_markEnd_pos.X)
			{
				tempStart->X = g_markStart_pos.X;
				tempEnd->X = g_markEnd_pos.X;
			}
			else
			{
				tempStart->X = g_markEnd_pos.X;
				tempEnd->X = g_markStart_pos.X;
			}

			if (g_markStart_pos.Y < g_markEnd_pos.Y)
			{
				tempStart->Y = g_markStart_pos.Y;
				tempEnd->Y = g_markEnd_pos.Y;
			}
			else
			{
				tempStart->Y = g_markEnd_pos.Y;
				tempEnd->Y = g_markStart_pos.Y;
			}
		}
		else
		{
			tempStart->X = g_markStart_pos.X;
			tempStart->Y = g_markStart_pos.Y;
			tempEnd->X = g_markStart_pos.X;
			tempEnd->Y = g_markStart_pos.Y;
		}
	}
}

void normalMarkedLinePos(COORD *tempStart, COORD *tempEnd)
{
	if ((g_markStart_pos.X != -1) && (g_markStart_pos.Y != -1))
	{
		tempStart->X = 0;
		tempEnd->X = 0;
		tempStart->Y = g_markStart_pos.Y;

		if ((g_markEnd_pos.X != -1) && (g_markEnd_pos.Y != -1))
			tempEnd->Y = g_markEnd_pos.Y;
		else
			tempEnd->Y = g_markStart_pos.Y;

		if (tempStart->Y > tempEnd->Y)
		{
			tempStart->Y = g_markEnd_pos.Y;
			tempEnd->Y = g_markStart_pos.Y;
		}
	}
}

void normalMarkedCharPos(COORD *tempStart, COORD *tempEnd)
{
	tempStart->X = g_markStart_pos.X;
	tempStart->Y = g_markStart_pos.Y;

	if ((g_markEnd_pos.X != -1) && (g_markEnd_pos.Y != -1))
	{
		if ((g_markStart_pos.Y > g_markEnd_pos.Y) || ((g_markStart_pos.Y == g_markEnd_pos.Y) && (g_markStart_pos.X > g_markEnd_pos.X)))
		{
			tempStart->X = g_markEnd_pos.X;
			tempStart->Y = g_markEnd_pos.Y;
			tempEnd->X = g_markStart_pos.X;
			tempEnd->Y = g_markStart_pos.Y;
		}
		else
		{
			tempEnd->X = g_markEnd_pos.X;
			tempEnd->Y = g_markEnd_pos.Y;
		}
	}
	else
	{
		tempEnd->X = g_markStart_pos.X;
		tempEnd->Y = g_markStart_pos.Y;
	}
}

int isInMarkedSection()
{
	int isMarked = 0;
	COORD tempStart, tempEnd;

	if (isValidCoord())
	{
		if (g_Marked_Mode == Marked_Block)
		{
			int indexY = 0;

			normalMarkedBlockPos(&tempStart, &tempEnd);

			indexY = tempEnd.Y - tempStart.Y;

			if (((g_file_pos.Y + indexY) >= tempStart.Y) && (g_file_pos.Y <= tempEnd.Y) && (g_file_pos.X <= tempEnd.X))
				isMarked = 1;
		}
		else if (g_Marked_Mode == Marked_Line)
		{
			normalMarkedLinePos(&tempStart, &tempEnd);

			if ((g_file_pos.Y >= tempStart.Y) && (g_file_pos.Y <= tempEnd.Y))
				isMarked = 1;
		}
		else if (g_Marked_Mode == Marked_Char)
		{
			normalMarkedCharPos(&tempStart, &tempEnd);

			if ((g_file_pos.Y >= tempStart.Y) && (g_file_pos.Y <= tempEnd.Y))
			{
				isMarked = 1;

				if (((g_file_pos.Y == tempStart.Y) && (g_file_pos.X < tempStart.X)) || ((g_file_pos.Y == tempEnd.Y) && (g_file_pos.X > tempEnd.X)))
					isMarked = 0;
			}
		}
	}

	return isMarked;
}

char * copyMarkedBlock()
{
	char * result = NULL;
	COORD tempStart, tempEnd;
	Content * ptr = g_currentFile->g_content;
	int i = 0, j = 0, resultSize = 0;

	normalMarkedBlockPos(&tempStart, &tempEnd);

	for (i = 0; i <= tempEnd.Y; i++, ptr = ptr->next)
	{
		if (!ptr)
		{
			if (i < tempStart.Y)
				i = tempStart.Y;

			for (; i <= tempEnd.Y; i++)
			{
				for (j = tempStart.X; j <= tempEnd.X; j++)
				{
					resultSize = 2;

					if (result != NULL)
						resultSize += strlen(result);

					result = (char*)realloc(result, (sizeof(char) * resultSize));

					result[resultSize - 2] = ' ';
					result[resultSize - 1] = '\0';
				}
			}

			if (result == NULL)
			{
				result = (char*)realloc(result, (sizeof(char) * 1));
				result[0] = '\0';
			}
			else if (result[strlen(result)] != '\0')
			{
				result = (char*)realloc(result, (sizeof(char) * (strlen(result) + 1)));
				result[strlen(result)] = '\0';
			}

			break;
		}

		// Find the corresponding line
		if (i >= tempStart.Y)
		{
			for (j = tempStart.X; j <= tempEnd.X; j++)
			{
				resultSize = 2;

				if (result != NULL)
					resultSize += strlen(result);

				result = (char*)realloc(result, (sizeof(char) * resultSize));

				if ((ptr->data) && (j <= (int)strlen(ptr->data)) && (ptr->data[j] != '\0'))
					result[resultSize - 2] = ptr->data[j];
				else
					result[resultSize - 2] = ' ';

				result[resultSize - 1] = '\0';
			}
		}
	}

	return result;
}

void pasteByMarkedBlock(char * inputdata, int eidtMode)
{
	COORD tempStart, tempEnd;
	Content * ptr = g_currentFile->g_content;
	int tempEditMode = g_Edit_Mode, i = 0, j = 0, k = 0;

	if (inputdata != NULL)
	{
		normalMarkedBlockPos(&tempStart, &tempEnd);

		g_Edit_Mode = eidtMode;

		for (j = tempStart.Y; j <= tempEnd.Y; j++)
		{
			// Paste data
			for (k = tempStart.X; k <= tempEnd.X; k++)
			{
				if (inputdata[i] == '\n')
					inputChar(' ');
				else
					inputChar(inputdata[i]);
				i++;

				if (eidtMode == Replace_Mode)
					shiftR();

				if (i >= (int)strlen(inputdata))
					break;
			}

			// Move cursor
			if (j != tempEnd.Y)
			{
				for (; k > tempStart.X; k--)
					shiftL();

				shiftDn();
			}
			else if (eidtMode == Replace_Mode)
				shiftL();
		}

		g_Edit_Mode = tempEditMode;
	}
}

char * copyMarkedLine()
{
	char * result = NULL;
	COORD tempStart, tempEnd;
	Content * ptr = g_currentFile->g_content;
	int i = 0, resultSize = 0;

	normalMarkedLinePos(&tempStart, &tempEnd);

	for (i = 0; i <= tempEnd.Y; i++, ptr = ptr->next)
	{
		if (!ptr)
		{
			if (i < tempStart.Y)
				i = tempStart.Y;

			for (; i <= tempEnd.Y; i ++)
			{
				resultSize = 2;

				if (result != NULL)
					resultSize += strlen(result);

				result = (char*)realloc(result, (sizeof(char) * resultSize));
				result[resultSize - 2] = '\n';
				result[resultSize - 1] = '\0';
			}

			break;
		}

		// Find the corresponding line
		if (i >= tempStart.Y)
		{
			resultSize = 1;

			if (result != NULL)
				resultSize += strlen(result);

			if (ptr->data)
			{
				resultSize += strlen(ptr->data);

				result = (char*)realloc(result, (sizeof(char) * resultSize));

				memcpy(&(result[resultSize - (strlen(ptr->data)) - 1]), ptr->data, (strlen(ptr->data) + 1));
			}
			else
			{
				resultSize += 1;
				result = (char*)realloc(result, (sizeof(char) * resultSize));

				result[resultSize - 2] = '\n';
				result[resultSize - 1] = '\0';
			}
		}
	}

	return result;
}

char * copyMarkedChar()
{
	char * result = NULL;
	COORD tempStart, tempEnd;
	Content * ptr = g_currentFile->g_content;
	int i = 0, j = 0, resultSize = 0;

	normalMarkedCharPos(&tempStart, &tempEnd);

	for (i = 0; i <= tempEnd.Y; i++, ptr = ptr->next)
	{
		if (!ptr)
			break;

		// Find the corresponding line
		if (i >= tempStart.Y)
		{
			if (!(ptr->data))
			{
				if (result == NULL)
					resultSize = 2;
				else
					resultSize = strlen(result) + 2;

				result = (char*)realloc(result, (sizeof(char) * resultSize));

				result[resultSize - 2] = '\n';
				result[resultSize - 1] = '\0';
			}
			else
			{
				if ((i == tempStart.Y) || (i == tempEnd.Y))
				{
					for (j = 0; j <= (int)strlen(ptr->data); j++)
					{
						if (((i == tempEnd.Y) && (j > tempEnd.X)))
							break;

						if ((i == tempStart.Y) && (j < tempStart.X))
							continue;

						resultSize = 2;

						if (result != NULL)
							resultSize += strlen(result);

						result = (char*)realloc(result, (sizeof(char) * resultSize));

						result[resultSize - 2] = ptr->data[j];
						result[resultSize - 1] = '\0';
					}
				}
				else
				{
					resultSize = strlen(ptr->data) + 1;

					if (result != NULL)
						resultSize += strlen(result);

					result = (char*)realloc(result, (sizeof(char) * resultSize));

					memcpy(&(result[resultSize - (strlen(ptr->data)) - 1]), ptr->data, (strlen(ptr->data) + 1));
				}
			}
		}
	}

	return result;
}

int shouleCorrectMarkedSection()
{
	COORD tempStart, tempEnd;
	int shouldCorrect = 0;

	if (g_Marked_Mode == Marked_Line)
	{
		normalMarkedLinePos(&tempStart, &tempEnd);

		if (g_file_pos.Y < tempStart.Y)
			shouldCorrect = 1;
	}
	else if (g_Marked_Mode == Marked_Char)
	{
		normalMarkedCharPos(&tempStart, &tempEnd);

		if ((g_file_pos.Y < tempStart.Y) || ((g_file_pos.Y == tempStart.Y) && (g_file_pos.X < tempStart.X)))
			shouldCorrect = 1;
	}

	return shouldCorrect;
}

void pasteByMarkedLineAndChar(char * inputdata)
{
	COORD tempStart, tempEnd, * start = NULL, * end = NULL;
	int tempEditMode = g_Edit_Mode, i = 0, shouldCorrect = 0, indexX = 0;

	if (inputdata != NULL)
	{
		g_Edit_Mode = Insert_Mode;

		// Check if marked section would be changed
		shouldCorrect = shouleCorrectMarkedSection();

		if (shouldCorrect)
		{
			if (g_Marked_Mode == Marked_Line)
				normalMarkedLinePos(&tempStart, &tempEnd);
			else if (g_Marked_Mode == Marked_Char)
				normalMarkedCharPos(&tempStart, &tempEnd);

			if ((tempStart.X == g_markStart_pos.X) && (tempStart.Y == g_markStart_pos.Y))
			{
				start = &g_markStart_pos;
				end = &g_markEnd_pos;
			}
			else
			{
				start = &g_markEnd_pos;
				end = &g_markStart_pos;
			}

			indexX = tempStart.X - g_file_pos.X;
		}

		// Paste data
		for (i = 0; i < (int)strlen(inputdata); i++)
		{
			if (inputdata[i] == '\n')
			{
				if (shouldCorrect)
				{
					if (g_file_pos.Y == start->Y)
						start->X = indexX;

					start->Y += 1;

					if ((end->X != -1) && (end->Y != -1))
					{
						if (g_file_pos.Y == end->Y)
							end->X = indexX + (tempEnd.X - tempStart.X + 1);

						end->Y += 1;
					}
				}

				enter();
			}
			else
			{
				if (shouldCorrect)
				{
					if (g_file_pos.Y == start->Y)
						start->X += 1;
					if ((end->X != -1) && (end->Y != -1) && (g_file_pos.Y == end->Y))
						end->X += 1;
				}

				inputChar(inputdata[i]);
			}
		}

		g_Edit_Mode = tempEditMode;
	}
}

/* Input : z -> Insert_Mode ; o -> Replace_Mode */
int copyMarkedtoCurrentPos(char input)
{
	int isSuccess = 0;
	char * temp = NULL;

	if (isValidCoord() && (g_markStart_pos.X != -1) && (g_markStart_pos.Y != -1))
	{
		if (isInMarkedSection())
			perr("Source and target conflict!");
		else if ((tolower(input) == 'o') && (g_Marked_Mode != Marked_Block))
			perr("Support Block-Marked Mode only.");
		else
		{
			if (g_Marked_Mode == Marked_Block)
			{
				temp = copyMarkedBlock();

				if (tolower(input) == 'o')
					pasteByMarkedBlock(temp, Replace_Mode);
				else
					pasteByMarkedBlock(temp, Insert_Mode);

				isSuccess = 1;
			}
			else if ((g_Marked_Mode == Marked_Line) || (g_Marked_Mode == Marked_Char))
			{
				if (g_Marked_Mode == Marked_Line)
					temp = copyMarkedLine();
				else
					temp = copyMarkedChar();

				pasteByMarkedLineAndChar(temp);

				isSuccess = 1;
			}
		}
	}

	if (temp)
		free(temp);

	return isSuccess;
}

int moveToFilePos(COORD destCoord)
{
	COORD tempFilePos = g_file_pos;
	int isValidDest = 0;
	int tempMode = g_Current_Mode;

	if (isValidCoord())
	{
		// Change mode to Edit Mode
		g_Current_Mode = g_Edit_Mode;

		// Check the destination coord is valid
		g_file_pos = destCoord;

		if (isValidCoord())
			isValidDest = 1;
		else
			perr("Destination coord is invalid.");

		g_file_pos = tempFilePos;

		// Move to destination coord
		if (isValidDest)
		{
			while ((g_file_pos.X != destCoord.X) || (g_file_pos.Y != destCoord.Y))
			{
				if (g_file_pos.X < destCoord.X)
					shiftR();
				else if (g_file_pos.X > destCoord.X)
					shiftL();

				if (g_file_pos.Y < destCoord.Y)
					shiftDn();
				else if (g_file_pos.Y > destCoord.Y)
					shiftUp();
			}
		}
	}

	g_Current_Mode = tempMode;

	return isValidDest;
}

void deleteMarkedText()
{
	COORD tempStart, tempEnd;
	char * temp = NULL;
	int i = 0, j = 0, k = 0;

	if (isValidCoord() && (g_markStart_pos.X != -1) && (g_markStart_pos.Y != -1))
	{
		if (g_Marked_Mode == Marked_Block)
		{
			temp = copyMarkedBlock();
			normalMarkedBlockPos(&tempStart, &tempEnd);

			if ((temp) && (moveToFilePos(tempStart)))
			{
				for (j = tempStart.Y; j <= tempEnd.Y; j++)
				{
					if (i >= (int)strlen(temp))
						break;

					for (k = tempStart.X; k <= tempEnd.X; k++)
					{
						if (i >= (int)strlen(temp))
							break;

						if (temp[i] == '\n')
						{
							i += (tempEnd.X - k + 1);
							break;
						}

						delete_char();
						i++;
					}

					if (j != tempEnd.Y)
						shiftDn();
				}

				for (j = j - 1; j > tempStart.Y; j--)
					shiftUp();
			}
		}
		else if (g_Marked_Mode == Marked_Line)
		{
			normalMarkedLinePos(&tempStart, &tempEnd);

			if (moveToFilePos(tempStart))
			{
				for (i = tempStart.Y; i <= tempEnd.Y; i++)
				{
					erase(0);
					delete_char();
				}
			}
		}
		else if (g_Marked_Mode == Marked_Char)
		{
			temp = copyMarkedChar();
			normalMarkedCharPos(&tempStart, &tempEnd);
			
			if ((temp) && (moveToFilePos(tempStart)))
				for (i = 0; i < (int)strlen(temp); i++)
					delete_char();
		}
	}

	cleanMarkPos();

	if (temp)
		free(temp);
}

void moveMarkedtoCurrentPos()
{
	COORD tempFilePos, tempStart, tempEnd;

	if (isValidCoord() && (g_markStart_pos.X != -1) && (g_markStart_pos.Y != -1))
	{
		tempFilePos = g_file_pos;

		if (copyMarkedtoCurrentPos('z'))
		{
			if (g_Marked_Mode == Marked_Block)
			{
				tempFilePos = g_file_pos;
			}
			else if ((g_Marked_Mode == Marked_Line) || (g_Marked_Mode == Marked_Char))
			{
				if (g_Marked_Mode == Marked_Line)
					normalMarkedLinePos(&tempStart, &tempEnd);
				else
					normalMarkedCharPos(&tempStart, &tempEnd);

				if (g_file_pos.Y < tempStart.Y)
					tempFilePos = g_file_pos;
				else
					tempFilePos.X = g_file_pos.X;
			}

			deleteMarkedText();
			moveToFilePos(tempFilePos);
		}
	}
}

void markedString(COORD startPos, COORD endPos)
{
	COORD tempCurrentFilePos = g_file_pos;

	g_file_pos = startPos;
	setMarkPos('c');
	g_file_pos = endPos;
	setMarkPos('c');

	g_file_pos = tempCurrentFilePos;
}

/* Input : data -> Input string                             */
/*         isFound -> 0 => First time , 1 => Keep searching */
void findString(char * data)
{
	COORD startS, endS, tempFilePos;
	Content * ptr = g_currentFile->g_content;
	int i = 0, j = 0, k = 0;

	if (data)
	{
		// Initial start point for searching
		if (g_findStringFlag)
		{
			tempFilePos.Y = g_markEnd_pos.Y;
			tempFilePos.X = g_markEnd_pos.X + 1;
			g_findStringFlag = 0;
		}
		else
		{
			tempFilePos.X = 0;
			tempFilePos.Y = 0;
		}

		cleanMarkPos();

		// Search by first character
		for (i = 0; ptr; i++, ptr = ptr->next)
		{
			if ((i >= tempFilePos.Y) && (ptr->data) && (strlen(ptr->data) >= strlen(data)))
			{
				if (i == tempFilePos.Y)
					j = tempFilePos.X;
				else
					j = 0;

				for (; j < (int)strlen(ptr->data); j++)
				{
					if (ptr->data[j] == data[0])
					{
						g_findStringFlag = 1;

						for (k = 1; k < (int)strlen(data); k++)
						{
							if (ptr->data[j + k] != data[k])
							{
								g_findStringFlag = 0;
								break;
							}
						}

						// If found, marked string
						if (g_findStringFlag == 1)
						{
							startS.X = j;
							startS.Y = i;
							endS.X = j + k - 1;
							endS.Y = i;

							markedString(startS, endS);

							// Change current mode to Edit Mode
							g_Current_Mode = g_Edit_Mode;

							break;
						}
					}					
				}

				if (g_findStringFlag == 1)
					break;
			}
		}

		if (g_findStringFlag == 1)
			moveToFilePos(startS);
		else
			perr("Cannot find this string.");
	}
}

void keepFindString()
{
	if (g_findStringFlag || g_ReplaceMarkedEnableFlag)
		doCurrentCommand();
}

void changeBlockMode(int pressEsc)
{
	if((g_Current_Mode != Command_Mode) && pressEsc)        // In Edit    mode and pressed Esc
		g_Current_Mode = Command_Mode ;
	else if((g_Current_Mode == Command_Mode) && pressEsc)   // In Command mode and pressed Esc
		g_Current_Mode = g_Edit_Mode ;
	else if(g_Edit_Mode == Replace_Mode)                    // In Replace mode and pressed Insert
		g_Edit_Mode = Insert_Mode ;
	else                                                    // In Insert  mode and pressed Insert
		g_Edit_Mode = Replace_Mode ;
}

void updateCurrentContent(int isEdit)
{
	int windowY = g_file_pos.Y - g_cursor_pos.Y , windowX = g_file_pos.X - g_cursor_pos.X , i = 0, j = 0 ;
	Content * ptr = g_currentFile->g_content;
	HANDLE hout = GetStdHandle(STD_OUTPUT_HANDLE) ;
	DWORD dwTT ;
	COORD temp ;

	// Normalization the upper left corner of window 
	if(windowY < 0)
		windowY = 0 ;
	if(windowX < 0)
		windowX = 0 ;

	// Check if need to update
	if((isEdit) || (g_window_pos.X != windowX) || (g_window_pos.Y != windowY))
	{
		// Initial window
		for(i = 0 ; i <= endLine_per_page ; i++)
		{
			temp.X = 0 ;
			temp.Y = i ;
			FillConsoleOutputCharacter(hout, ' ', (endCol_per_line + 1), temp, &dwTT) ;
			FillConsoleOutputAttribute(hout, BACKGROUND_RED & BACKGROUND_GREEN & BACKGROUND_BLUE, (endCol_per_line + 1), temp, &dwTT);
		}

		// Show text in image of window
		for(i = 0 ; i <= (windowY + endLine_per_page) ; i++, ptr = ptr->next)
		{
			if(!ptr)
				break ;

			if((i >= windowY) && (ptr->data) && ((int)(strlen(ptr->data)) > windowX))
			{
				for(j = windowX ; j <= (windowX + endCol_per_line) ; j++)
				{
					if((int)(strlen(ptr->data)) <= j)
						break ;

					temp.Y = i - windowY ;
					temp.X = j - windowX ;
					SetConsoleCursorPosition(hout, temp) ;
					WriteConsole(hout, &(ptr->data[j]), 1, &dwTT, NULL) ;
				}
			}
		}
	}

	g_window_pos.X = windowX ;
	g_window_pos.Y = windowY ;
}

void updateCurrentMode()
{
	HANDLE hout = GetStdHandle(STD_OUTPUT_HANDLE) ;
	DWORD dwTT ;
	COORD temp ;

	temp.X = startCol_message_EditMode ;
	temp.Y = message_line ;

	SetConsoleCursorPosition(hout, temp) ;
	FillConsoleOutputCharacter(hout, ' ', 9, temp, &dwTT) ;

	// Update text of edit mode
	switch(g_Edit_Mode)
	{
		case Insert_Mode :
			WriteConsole(hout, "Insert", strlen("Insert"), &dwTT, NULL) ;
			break ;
		case Replace_Mode :
			WriteConsole(hout, "Replace", strlen("Replace"), &dwTT, NULL) ;
			break ;		
		default :
			perr("Invalid mode!") ;
			break ;
	}
}

void updateCurrentCoord()
{
	char statusCoord[50], temp[10] ;
	HANDLE hout = GetStdHandle(STD_OUTPUT_HANDLE) ;
	DWORD dwTT ;
	COORD statusCoord_pos ;

	// Update text of coordinate
	memset(statusCoord, 0x00, sizeof(char) * 50) ;
	memset(temp, 0x00, sizeof(char) * 10) ;

	if(g_Current_Mode == Command_Mode)
		strcpy_s(statusCoord, 50, "0 , 0") ;
	else
	{
		_itoa_s(g_file_pos.X, statusCoord, 50, 10) ;
		_itoa_s(g_file_pos.Y, temp, 10, 10) ;
		strcat_s(statusCoord, 50, " , ") ;
		strcat_s(statusCoord, 50, temp) ;
	}

	statusCoord_pos.X = startCol_message_coord ;
	statusCoord_pos.Y = message_line ;
	SetConsoleCursorPosition(hout, statusCoord_pos) ;
	WriteConsole(hout, statusCoord, strlen(statusCoord), &dwTT, NULL) ;

	statusCoord_pos.X = startCol_message_coord + strlen(statusCoord) ;
	FillConsoleOutputCharacter(hout, ' ', ((startCol_message_EditMode - startCol_message_coord) - strlen(statusCoord)), statusCoord_pos, &dwTT) ;

	// Update current cursor position
	if(g_Current_Mode != Command_Mode)
		SetConsoleCursorPosition(hout, g_cursor_pos) ;
	else
		SetConsoleCursorPosition(hout, g_command_pos) ;
}

void updateCurrentCommand()
{
	HANDLE hout = GetStdHandle(STD_OUTPUT_HANDLE) ;
	DWORD dwTT ;
	COORD temp ;
	
	temp.X = 0 ;
	temp.Y = command_line ;

	FillConsoleOutputCharacter(hout, ' ', endCol_per_line + 1, temp, &dwTT) ;

	// Text
	if(g_command)
	{
		SetConsoleCursorPosition(hout, temp) ;
		WriteConsole(hout, g_command, strlen(g_command), &dwTT, NULL) ;
	}

	// Image
	FillConsoleOutputAttribute(hout, BACKGROUND_GREEN, (endCol_per_line + 1), temp, &dwTT) ;
}

void updateCurrentFileName()
{
	HANDLE hout = GetStdHandle(STD_OUTPUT_HANDLE) ;
	DWORD dwTT ;
	COORD temp ;

	temp.X = 0 ;
	temp.Y = message_line ;

	FillConsoleOutputCharacter(hout, ' ', startCol_message_coord, temp, &dwTT) ;
	SetConsoleCursorPosition(hout, temp) ;
	WriteConsole(hout, g_currentFile->g_fileName, strlen(g_currentFile->g_fileName), &dwTT, NULL);
}

int isInWindow(COORD pos)
{
	int result = 0;

	if ((pos.X >= g_window_pos.X) && (pos.X <= (g_window_pos.X + endCol_per_line)) && (pos.Y >= g_window_pos.Y) && (pos.Y <= (g_window_pos.Y + endLine_per_page)))
		result = 1;

	return result;
}

void updateCurrentMarkedBlock()
{
	HANDLE hout = GetStdHandle(STD_OUTPUT_HANDLE) ;
	DWORD dwTT ;
	COORD tempStart, tempEnd, temp ;
	int markedSize = 0, i = 0, isShow = 0 ;

	// Find the marked block
	if((g_markStart_pos.X != -1) && (g_markStart_pos.Y != -1))
	{
		// Normalization the marked coordinate
		normalMarkedBlockPos(&tempStart, &tempEnd);
		
		// Check if need to show
		if (isInWindow(tempStart) && isInWindow(tempEnd))
			isShow = 1;
		else if(isInWindow(tempStart) && !isInWindow(tempEnd))
		{
			if(tempEnd.X > (g_window_pos.X + endCol_per_line) )
				tempEnd.X = g_window_pos.X + endCol_per_line ;
			if(tempEnd.Y > (g_window_pos.Y + endLine_per_page) )
				tempEnd.Y = g_window_pos.Y + endLine_per_page ;

			isShow = 1;
		}
		else if(!isInWindow(tempStart) && isInWindow(tempEnd))
		{
			if(tempStart.X < g_window_pos.X)
				tempStart.X = g_window_pos.X ;
			if(tempStart.Y < g_window_pos.Y)
				tempStart.Y = g_window_pos.Y ;

			isShow = 1;
		}
		else if (!isInWindow(tempStart) && !isInWindow(tempEnd))
		{
			if ((tempStart.Y < g_window_pos.Y) && (tempEnd.Y > (g_window_pos.Y + endLine_per_page)))
			{
				tempStart.Y = g_window_pos.Y;
				tempEnd.Y = g_window_pos.Y + endLine_per_page;

				isShow = 1;
			}

			if ((tempStart.X < g_window_pos.X) && (tempEnd.X >(g_window_pos.X + endCol_per_line)))
			{
				tempStart.X = g_window_pos.X;
				tempEnd.X = g_window_pos.X + endCol_per_line;

				isShow = 1;
			}
		}

		// Show marked text

		if (isShow)
		{
			markedSize = tempEnd.X - tempStart.X + 1 ;
			
			temp.X = tempStart.X - g_window_pos.X ;

			for (i = tempStart.Y; i <= tempEnd.Y; i++)
			{
				temp.Y = i - g_window_pos.Y;
				FillConsoleOutputAttribute(hout, BACKGROUND_RED | BACKGROUND_BLUE, markedSize, temp, &dwTT);
			}
		}
	}
}

void updateCurrentMarkedLine()
{
	HANDLE hout = GetStdHandle(STD_OUTPUT_HANDLE) ;
	DWORD dwTT ;
	COORD temp, tempStart, tempEnd ;
	int i = 0, start = 0, end = 0, isShow = 1;

	if ((g_markStart_pos.X != -1) && (g_markStart_pos.Y != -1))
	{
		// Normalization the marked coordinate
		normalMarkedLinePos(&tempStart, &tempEnd);

		// Show marked text
		temp.X = 0;

		if ((tempStart.Y > (g_window_pos.Y + endLine_per_page)) || (tempEnd.Y < g_window_pos.Y))
			isShow = 0;

		if (isShow)
		{
			if (tempStart.Y < g_window_pos.Y)
				start = g_window_pos.Y;
			else
				start = tempStart.Y;

			if (tempEnd.Y >(g_window_pos.Y + endLine_per_page))
				end = g_window_pos.Y + endLine_per_page;
			else
				end = tempEnd.Y;

			for (i = start; i <= end; i++)
			{
				temp.Y = i - g_window_pos.Y;
				FillConsoleOutputAttribute(hout, BACKGROUND_RED | BACKGROUND_BLUE, (endCol_per_line + 1), temp, &dwTT);
			}
		}
	}
}

void updateCurrentMarkedChar()
{
	HANDLE hout = GetStdHandle(STD_OUTPUT_HANDLE);
	DWORD dwTT;
	COORD temp, tempStart, tempEnd ;
	Content * ptr = g_currentFile->g_content;
	int i = 0, j = 0;

	if ((g_markStart_pos.X != -1) && (g_markStart_pos.Y != -1))
	{
		// Normalization the marked coordinate
		normalMarkedCharPos(&tempStart, &tempEnd);

		// Show marked text
		for (i = 0; i <= (g_window_pos.Y + endLine_per_page); i++, ptr = ptr->next)
		{
			if (!ptr)
				break;

			if ((i >= g_window_pos.Y) && (ptr->data) && ((int)(strlen(ptr->data)) > g_window_pos.X))
			{
				if (tempEnd.Y < i)
					break;

				if (i >= tempStart.Y)
				{
					for (j = g_window_pos.X; j <= (g_window_pos.X + endCol_per_line); j++)
					{
						if ((int)(strlen(ptr->data)) <= j)
							break;

						if (((i == tempStart.Y) && (j < tempStart.X)) || ((i == tempEnd.Y) && (j > tempEnd.X)))
							continue;
						else
						{
							temp.Y = i - g_window_pos.Y;
							temp.X = j - g_window_pos.X;
							FillConsoleOutputAttribute(hout, BACKGROUND_RED | BACKGROUND_BLUE, 1, temp, &dwTT);
						}
					}
				}
			}
		}
	}
}

void updateMarkedControler()
{
	if ((g_markStart_pos.X != -1) && (g_markStart_pos.Y != -1))
	{
		switch (g_Marked_Mode)
		{
			case Marked_Block:
				updateCurrentMarkedBlock();
				break;
			case Marked_Line:
				updateCurrentMarkedLine();
				break;
			case Marked_Char:
				updateCurrentMarkedChar();
				break;
			default:
				break;
		}
	}
}

void updateControler(int isEdit, int isMoved, int isCommandUpdated, int editModeChanged, int * isCleanErrMsg)
{
	if ((isEdit) || ((g_Current_Mode != Command_Mode) && (isMoved)))
	{
		updateCurrentContent(isEdit) ;
		updateMarkedControler();
	}
	 
	if ((g_Current_Mode == Command_Mode) && (isCommandUpdated))
	{
		updateCurrentCommand() ;

		if (g_currentFile->g_fileName)
			updateCurrentFileName() ;
	}

	if(editModeChanged)
		updateCurrentMode() ;
	
	if (isEdit || isMoved || editModeChanged || isCommandUpdated)
		updateCurrentCoord() ;

	if(*isCleanErrMsg && (isEdit || isMoved || editModeChanged || isCommandUpdated))
	{
		eraseErrMsg() ;
		*isCleanErrMsg = 0 ;
	}

	if(g_errorFlag)
	{
		*isCleanErrMsg = 1 ;
		g_errorFlag = 0 ;
	}
}

void showAuthorMessage()
{
	HANDLE hout = GetStdHandle(STD_OUTPUT_HANDLE);
	DWORD dwTT;
	COORD tempCursorPos;

	tempCursorPos.X = 25;
	tempCursorPos.Y = 6;
	SetConsoleCursorPosition(hout, tempCursorPos);
	WriteConsole(hout, "* UEdit - Unlimited editor *", strlen("* UEdit - Unlimited editor *"), &dwTT, NULL);

	tempCursorPos.X = 17;
	tempCursorPos.Y = 9;
	SetConsoleCursorPosition(hout, tempCursorPos);
	WriteConsole(hout, "Open file :         e [[drive:][path]filename]", strlen("Open file :         e [[drive:][path]filename]"), &dwTT, NULL);
	tempCursorPos.X = 17;
	tempCursorPos.Y = 10;
	SetConsoleCursorPosition(hout, tempCursorPos);
	WriteConsole(hout, "Assign file name :  n [[drive:][path]filename]", strlen("Assign file name :  n [[drive:][path]filename]"), &dwTT, NULL);

	tempCursorPos.X = 25;
	tempCursorPos.Y = 13;
	SetConsoleCursorPosition(hout, tempCursorPos);
	WriteConsole(hout, "Author : Cindy, Chiang Hsieh", strlen("Author : Cindy, Chiang Hsieh"), &dwTT, NULL);

	tempCursorPos.X = 32;
	tempCursorPos.Y = 15;
	SetConsoleCursorPosition(hout, tempCursorPos);
	WriteConsole(hout, "2015 / 8 / 26", strlen("2015 / 8 / 26"), &dwTT, NULL);
	tempCursorPos.X = 35;
	tempCursorPos.Y = 17;
	SetConsoleCursorPosition(hout, tempCursorPos);
	WriteConsole(hout, "£»£¾£»", strlen("£»£¾£»"), &dwTT, NULL);
}

void setFileName(char * fileName)
{
	char currentFileDir[MAX_PATH] = {0}, *tempFileName = NULL, *p = NULL;
	int i = 0;
	
	if (fileName)
	{
		// Set directory if file name without it
		tempFileName = setCurrentDirtoFileName(fileName);

		// Set current file name
		g_currentFile->g_fileName = (char *)realloc(g_currentFile->g_fileName, (sizeof(char) * (strlen(tempFileName) + 1)));
		memset(g_currentFile->g_fileName, 0x00, (sizeof(char) * (strlen(tempFileName) + 1)));
		memcpy(g_currentFile->g_fileName, tempFileName, (sizeof(char) * strlen(tempFileName)));
		g_currentFile->g_fileName[strlen(tempFileName)] = '\0';
	}
}

int openFile(int argc, char * fileName)
{
	FILE * fp = NULL ;
	int isSuccess = 0, i = 0, j = 0 ;
	long int fileSize = 0 ;
	unsigned long pos = 0 ;
	char * temp = NULL ;
	Content * ptr = NULL, * ptr_end = NULL ;
	errno_t err ;

	// Add new file next to current file, and move current ptr to new file
	addNewFile();
	
	ptr = g_currentFile->g_content;

	// Read file
	if(argc == 1)
		perr("No input file!") ;
	else
	{
		err = fopen_s(&fp, fileName, "r+");

		if (err != 0)
			err = fopen_s(&fp, fileName, "w+");

		if (err != 0)
			perr("Open file failed!") ;
		else
		{
			setFileName(fileName) ;

			isSuccess = 1 ;

			// Get the size of file
			pos = ftell(fp) ; 
			fseek(fp, 0L, SEEK_END) ;
			fileSize = ftell(fp) ;
			fseek(fp, pos, SEEK_SET) ;

			if(fileSize > 0)
			{
				// Get total file into g_content
				temp = (char*)malloc(sizeof(char) * (fileSize + 1)) ;
				memset(temp, 0x00, (sizeof(char) * (fileSize + 1))) ;

				while(fgets(temp, (fileSize + 1), fp) != NULL)
				{
					writeLineintoContent(ptr, temp);
					
					// If got "TAB"(0x09), transfer data to "    " -> (4 * ' ')
					for(i = 0 ;  ; i++)
					{
						if(ptr->data[i] == '\0')
							break ;
						else if(ptr->data[i] == 0x09)
						{
							ptr->data = (char*)realloc(ptr->data, sizeof(char) * (strlen(ptr->data) + 1 + 3)) ;
							memmove(&(ptr->data[i + 4]), &(ptr->data[i + 1]), sizeof(char) * (strlen(ptr->data) + 1 - (i + 1))) ;
							
							for(j = 0 ; j < 4 ; j++)
								ptr->data[i + j] = ' ' ;

							i += 3 ;
						}
					}

					ptr->next = newContent() ;
					ptr_end = ptr ;
					ptr = ptr->next ;
				}

				ptr_end->next = NULL ;
				free(ptr) ;
				free(temp) ;
			}

			fclose(fp) ;
		}
	}

	return isSuccess ;
}

void writeFile()
{
	FILE * fp = NULL ;
	Content * ptr = g_currentFile->g_content;
	errno_t err ;

	if (!g_currentFile->g_fileName)
		perr("No initial file.") ;
	else
	{
		err = fopen_s(&fp, g_currentFile->g_fileName, "w+");

		if (err != 0)
			perr("Open file failed!") ;
		else
		{
			fseek(fp, 0, SEEK_SET) ;

			while(ptr)
			{
				if(ptr->data)
					fwrite(ptr->data, sizeof(char), strlen(ptr->data), fp) ;
				else
					fprintf(fp, "\n") ;

				ptr = ptr->next ;
			}

			fprintf(fp, "\0") ;

			fclose(fp) ;
		}
	}
}

/* Close current file and move current ptr to next file */
void closeCurrentFile()
{
	FileData * temp = NULL;

	if (g_currentFile)
	{
		temp = g_currentFile;

		if (g_currentFile->g_fileName)
		{
			cleanFileName();
			g_currentFile->g_fileName = NULL;
		}
		if (g_currentFile->g_content)
		{
			cleanContent();
			g_currentFile->g_content = NULL;
		}
		if (g_currentFile->pre)
		{
			if (g_currentFile->pre == g_currentFile->next)
			{
				g_currentFile->pre->next = NULL;
				g_currentFile->pre->pre = NULL;
			}
			else
				g_currentFile->pre->next = g_currentFile->next;
		}
		if ((g_currentFile->next) && (g_currentFile->pre != g_currentFile->next))
			g_currentFile->next->pre = g_currentFile->pre;

		g_currentFile = g_currentFile->next;

		free(temp);
	}
}

void cleanMarkPos()
{
	g_markStart_pos.X = -1 ;
	g_markStart_pos.Y = -1 ;
	g_markEnd_pos.X = -1 ;
	g_markEnd_pos.Y = -1 ;
	g_Marked_Mode = 0 ;
}

void cleanContent()
{
	Content * temp = NULL ;

	g_file_pos.X = 0 ;
	g_file_pos.Y = 0 ;

	while (g_currentFile->g_content)
	{
		temp = g_currentFile->g_content;

		if (g_currentFile->g_content->data)
		{
			free(g_currentFile->g_content->data);
			g_currentFile->g_content->data = NULL;
		}
		g_currentFile->g_content = g_currentFile->g_content->next;

		free(temp) ;
	}

	g_currentFile->g_content = NULL;
}

void cleanFile()
{
	while (g_currentFile)
		closeCurrentFile();

	g_currentFile = NULL;
}

void cleanCommand()
{
	g_command_pos.X = 0 ;

	if(g_command)
	{
		free(g_command) ;
		g_command = NULL ;
	}
}

void cleanFileName()
{
	if (g_currentFile->g_fileName)
	{
		free(g_currentFile->g_fileName);
		g_currentFile->g_fileName = NULL;
	}
}

void initConsole(int fileExist)
{
	int cleanErrMsg = 1 ;

	g_cursor_pos.X = 0 ;
	g_cursor_pos.Y = 0 ;
	g_file_pos.X = 0 ;
	g_file_pos.Y = 0 ;
	g_window_pos.X = -1 ;
	g_window_pos.Y = -1 ;
	g_command_pos.X = 0 ;
	g_command_pos.Y = command_line ;
	g_markStart_pos.X = -1 ;
	g_markStart_pos.Y = -1 ;
	g_markEnd_pos.X = -1 ;
	g_markEnd_pos.Y = -1 ;

	g_Current_Mode = Command_Mode ;
	g_Edit_Mode = Insert_Mode ;
	g_Marked_Mode = 0;

	g_errorFlag = 0;
	g_findStringFlag = 0;
	g_ReplaceMarkedEnableFlag = 0;
	g_initReplaceMarkedFlag = 0;
	g_waitAnsFlag = 0;
	g_getAnsFlag = 0;

	// Initial text
	if(fileExist == 1)
	{
		updateCurrentFileName() ;
		updateCurrentContent(1) ;
	}
	else
		showAuthorMessage();

	updateCurrentMode() ;
	updateCurrentCoord() ;	

	// Initial Command line
	updateCurrentCommand() ;

	// Initial Error message line
	eraseErrMsg() ;
}

void endConsole()
{
	HANDLE hout = GetStdHandle(STD_OUTPUT_HANDLE) ;

	g_cursor_pos.X = 0 ;
	g_cursor_pos.Y = error_message_line ;
	SetConsoleCursorPosition(hout, g_cursor_pos) ;
}

/*********************** Function List ***********************************************************************************************************/
/* Phase 1. => Cursor Up / Down / Left / Right , Home / End , PgUp / PgDn                                                                        */
/* Phase 2. => Ctrl + Home / End / PgUp / Left / Right , F5 / F6 , F4 , Insert , Enter , Del , Backspace                                         */
/* Phase 3. => ESC , F2 / F3 , e [[drive:][path]filename] , n [file name]                                                                        */
/* Phase 4. => Alt + B / C / L / U / Z / M / O / D , /String , c /str/new_str/ ( Shift + F5 ) , cd [drive:][path] , dir [drive:][path][filespec] */
/*************************************************************************************************************************************************/
void main(int argc, char *argv[])
{
	int more = 1, pressControl = 0, pressAlt = 0, pressShift = 0, editModeChanged = 0, fileExist = 0, isEdit = 0, isMoved = 0, isCommandUpdated = 0, isCleanErrMsg = 0;
	int waitAnsFlag = 1;
	char * statusFunction = NULL;
	HANDLE hin = GetStdHandle(STD_INPUT_HANDLE);
	static DWORD dwTT;
	static INPUT_RECORD input;
	
	// Open file and get the total content
	fileExist = openFile(argc, argv[1]);

	// Show in console
	initConsole(fileExist);

	// Get Event from KB Start ------------------------------------------------------------------------------------------
	while (more)
	{
		hin = GetStdHandle(STD_INPUT_HANDLE);
		pressControl = 0;
		pressAlt = 0;
		pressShift = 0;
		editModeChanged = 0;
		isEdit = 0;
		isMoved = 0;
		isCommandUpdated = 0;

		ReadConsoleInput(hin, &input, 1, &dwTT);

		if (!input.Event.KeyEvent.bKeyDown)
			continue;

		if (g_waitAnsFlag && (tolower(input.Event.KeyEvent.uChar.AsciiChar) != 'y') && (tolower(input.Event.KeyEvent.uChar.AsciiChar) != 'n'))
			continue;

		if ((LEFT_CTRL_PRESSED | RIGHT_CTRL_PRESSED) & input.Event.KeyEvent.dwControlKeyState)
			pressControl = 1;

		if ((LEFT_ALT_PRESSED | RIGHT_ALT_PRESSED) & input.Event.KeyEvent.dwControlKeyState)
			pressAlt = 1;

		if ((SHIFT_PRESSED)& input.Event.KeyEvent.dwControlKeyState)
			pressShift = 1;

		if (g_waitAnsFlag)
		{
			g_waitAnsFlag = 0;

			if (tolower(input.Event.KeyEvent.uChar.AsciiChar) == 'y')
			{
				g_getAnsFlag = 1;
				keepFindString();
				g_getAnsFlag = 0;
			}

			isCleanErrMsg = 1;

			isMoved = 1;
			isEdit = 1;
		}
		else if (pressShift && !pressControl && !pressAlt && (input.Event.KeyEvent.wVirtualKeyCode == VK_F5))  // If press Shift + F5
		{
			// If in "Find and Replace string" mode
			if (g_ReplaceMarkedEnableFlag)
			{
				g_waitAnsFlag = 1;
				doCurrentCommand();
				perr("Do you really want to replace? (Y/N) ");

				isCleanErrMsg = 0;

				isMoved = 1;
				isEdit = 1;
			}
		}
		else
		{
			switch (input.Event.KeyEvent.wVirtualKeyCode)
			{
			case VK_ESCAPE: // Switch Command/Edit mode
				if (!pressControl && !pressAlt)
				{
					changeBlockMode(1);
					isMoved = 1;
				}
				break;

			case VK_LEFT:   // Ctrl : Left 40 spaces ; w/o Ctrl : Shift left
				if ((g_Current_Mode != Command_Mode) && pressControl && !pressAlt)
				{
					ctrl_left();
					isMoved = 1;
				}
				else if (!pressControl && !pressAlt)
				{
					shiftL();
					isMoved = 1;
				}
				break;

			case VK_RIGHT:  // Ctrl : Right 40 spaces ; w/o Ctrl : Shift right
				if ((g_Current_Mode != Command_Mode) && pressControl && !pressAlt)
				{
					ctrl_right();
					isMoved = 1;
				}
				else if (!pressControl)
				{
					shiftR();
					isMoved = 1;
				}
				break;

			case VK_UP:     // Shift up
				if ((g_Current_Mode != Command_Mode) && !pressControl && !pressAlt)
				{
					shiftUp();
					isMoved = 1;
				}
				break;

			case VK_DOWN:   // Shift down
				if ((g_Current_Mode != Command_Mode) && !pressControl && !pressAlt)
				{
					shiftDn();
					isMoved = 1;
				}
				break;

			case VK_HOME:   // Ctrl : First line of file ; w/o Ctrl : Start of string
				if ((g_Current_Mode != Command_Mode) && !pressAlt)
				{
					if (pressControl)
						ctrl_home();
					else
						home();
					isMoved = 1;
				}
				break;

			case VK_END:    // Ctrl : End line of file ; w/o Ctrl : End of string
				if ((g_Current_Mode != Command_Mode) && !pressAlt)
				{
					if (pressControl)
						ctrl_end();
					else
						end();
					isMoved = 1;
				}
				break;

			case VK_PRIOR:  // Ctrl : Top of screen ; w/o Ctrl : PgUp (20 line before)
				if ((g_Current_Mode != Command_Mode) && !pressAlt)
				{
					if (pressControl)
						ctrl_prior();
					else
						prior();
					isMoved = 1;
				}
				break;

			case VK_NEXT:   // PgDn (20 line after)
				if ((g_Current_Mode != Command_Mode) && !pressControl && !pressAlt)
				{
					next();
					isMoved = 1;
				}
				break;

			case VK_F5:     // Erases all contents of line
				if ((g_Current_Mode != Command_Mode) && !pressControl && !pressAlt)
				{
					erase(1);
					isEdit = 1;
				}
				break;

			case VK_F6:     // Erases to end of linee
				if ((g_Current_Mode != Command_Mode) && !pressControl && !pressAlt)
				{
					erase(0);
					isEdit = 1;
				}
				break;

			case VK_INSERT: // Switch Insert/Replace mode
				if (!pressControl && !pressAlt)
				{
					changeBlockMode(0);
					editModeChanged = 1;
				}
				break;

			case VK_RETURN: // Enter -> Ctrl : Keep finding string ; w/o Ctrl => Next line
				if (!pressControl && !pressAlt)
				{
					enter();

					if (g_Current_Mode == Command_Mode)
						isCommandUpdated = 1;

					isEdit = 1;
					isMoved = 1;
				}
				else if (pressControl && !pressAlt)
				{
					if (g_findStringFlag || g_ReplaceMarkedEnableFlag)
					{
						keepFindString();

						isEdit = 1;
						isMoved = 1;
					}
				}
				break;

			case VK_DELETE: // Delete character
				if (!pressControl && !pressAlt)
				{
					delete_char();

					if (g_Current_Mode != Command_Mode)
						isEdit = 1;
					else
						isCommandUpdated = 1;
				}
				break;

			case VK_BACK:   // "<-" => Delete previous character
				if (!pressControl && !pressAlt)
				{
					backspace();

					isMoved = 1;
					if (g_Current_Mode != Command_Mode)
						isEdit = 1;
					else
						isCommandUpdated = 1;
				}
				break;

			case VK_F4:     // Quit without save current file
				if (!pressControl && !pressAlt)
				{
					closeCurrentFile();

					if (g_currentFile)
						initConsole(1);
					else
						more = 0;
				}
				break;

			case VK_F2:     // Saves current file
				if (!pressControl && !pressAlt)
					writeFile();
				break;

			case VK_F3:     // Saves and quit current file
				if (!pressControl && !pressAlt)
				{
					writeFile();
					closeCurrentFile();

					if (g_currentFile)
						initConsole(1);
					else
						more = 0;
				}
				break;

			default:        // Press Alt / Print content
				if ((g_Current_Mode != Command_Mode) && !pressControl && pressAlt && (isprint(input.Event.KeyEvent.uChar.AsciiChar)))
				{
					if (tolower(input.Event.KeyEvent.uChar.AsciiChar) == 'u')
						cleanMarkPos();
					else if ((tolower(input.Event.KeyEvent.uChar.AsciiChar) == 'z') || (tolower(input.Event.KeyEvent.uChar.AsciiChar) == 'o'))
						copyMarkedtoCurrentPos(input.Event.KeyEvent.uChar.AsciiChar);
					else if (tolower(input.Event.KeyEvent.uChar.AsciiChar) == 'd')
						deleteMarkedText();
					else if (tolower(input.Event.KeyEvent.uChar.AsciiChar) == 'm')
						moveMarkedtoCurrentPos();
					else
						setMarkPos(tolower(input.Event.KeyEvent.uChar.AsciiChar));

					isEdit = 1;
				}
				else if (!pressControl && !pressAlt && (isprint(input.Event.KeyEvent.uChar.AsciiChar)))
				{
					inputChar(input.Event.KeyEvent.uChar.AsciiChar);

					if (g_Current_Mode != Command_Mode)
						isEdit = 1;
					else
						isCommandUpdated = 1;
					isMoved = 1;
				}
				break;
			}
		}

		// Update current data in console
		if (more)
			updateControler(isEdit, isMoved, isCommandUpdated, editModeChanged, &isCleanErrMsg);
	}

	// Get Event from KB END --------------------------------------------------------------------------------------------

	// ~()
	cleanFile();
	cleanCommand();
	cleanMarkPos();
	eraseErrMsg();
	endConsole();

	system("PAUSE");
}