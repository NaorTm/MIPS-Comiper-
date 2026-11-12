#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define IF 0
#define ID 1
#define EX 2
#define MEM 3
#define WB 4
#define COMMAND_LEN 5
#define COMMANDS_NUM 13

typedef struct Control
{
	/*Executoion/ALU stage signals:*/
	int RegDst;
	int ALUOp0;
	int ALUOp1;
	int ALUSrc;
	/*Memory stage signals:*/
	int Branch;
	int MemRead;
	int MemWrite;
	/*Write back stage signals:*/
	int RegWrite;
	int MemToReg;
}Control;

typedef struct Command
{
	char type;			/*'R'=r type, 'I'=i type alu, 'L'=lw, 'S'=sw, 'B'=branch, 'J'=jump, '0'=bubble/stall*/
	char name[COMMAND_LEN];
	int address;
	int operand1;
	int operand2;
	int operand3;
	char command[30];
	Control signals;	/*control signals for the give command*/
}Command;

typedef struct Pipeline
{
	Command stage[5];
	int forward;
	int branch;
}Pipeline;

/*Global variables*/
char commands[][5] = { "add","sub","and","or","addi","subi","ori","andi","lw","sw","beq","bneq","j" }; //i=: 0-3 r type, 4-11 i type, 12-j type
Control rControl = { 1,1,0,0,0,0,0,1,0 };
Control lControl = { 0,0,0,1,0,1,0,1,1 };
Control sControl = { 0,0,0,1,0,0,1,0,0 };
Control bControl = { 0,0,1,0,1,0,0,0,0 };
Control iControl = { 0,0,0,1,0,0,0,1,0 };
Control stallControl = { 0,0,0,0,0,0,0,0,0 };
float cycle = 0, stalls = 0;
int instructions = 0;
int parseError = 0;

/*Managing functions*/
void nextCycle(FILE* traceFile, Pipeline* pipe);	//pipe[0]=pipe[1] in the end NEXT CLOCK CYCLE
char getCommandType(char* commandName);		//return 'R'=r type, 'I'=i type alu, 'L'=lw, 'S'=sw, 'B'=branch, 'J'=jump, '0'=bubble/stall
void stall(Pipeline* pipe, int location);	//pipe[location] will be stalled
Command addBubble();						//returns a bubble/stall
void printCPI();							//print CPI
void printfCycleInfo(Pipeline pipe);		//print current cycle pipeline stages and count stalls
void initPipe(Pipeline* pipe);				//initiate pipe to 'no commands'
int convertToBinary(int origin);			//convert given value to binary
void bStall(Pipeline* pipe);	//pipe[location] will be stalled
void flush(Pipeline* pipe);					//flush for forward=0
void flush1(Pipeline* pipe);				//flush for forward=1
void endFile(Pipeline* pipe);				//close file - add 4 nops

/*File functions*/
Command readCommand(FILE* filePtr);			//read a command line
Command endOfFile();						//nop command generate

//The main.
int main(int argc, char* argv[])
{
    Pipeline pipe;
    const char *path;
    char choice[16];
    if (argc < 3 || argc > 4 ||
        (strcmp(argv[1], "0") && strcmp(argv[1], "1")) ||
        (strcmp(argv[2], "0") && strcmp(argv[2], "1"))) {
        fprintf(stderr, "Usage: %s <forward: 0|1> <branch: 0|1> [trace-file]\n", argv[0]);
        return EXIT_FAILURE;
    }
    if (argc == 4) path = argv[3];
    else {
        printf("Enter 1 for trace1.txt, 2 for trace2.txt\n");
        if (!fgets(choice, sizeof choice, stdin) || (choice[0] != '1' && choice[0] != '2') || (choice[1] != '\n' && choice[1] != '\0')) {
            fprintf(stderr, "Invalid trace selection.\n");
            return EXIT_FAILURE;
        }
        path = choice[0] == '1' ? "trace1.txt" : "trace2.txt";
    }
    FILE *traceFile = fopen(path, "r");
    if (!traceFile) { perror(path); return EXIT_FAILURE; }
    initPipe(&pipe);
    pipe.forward = argv[1][0] - '0';
    pipe.branch = argv[2][0] - '0';
    nextCycle(traceFile, &pipe);
    fclose(traceFile);
    if (parseError) return EXIT_FAILURE;
    printCPI();
    return EXIT_SUCCESS;
}

/*Pipeline functions*/
void nextCycle(FILE* traceFile, Pipeline* pipe)
{
	int i, draining = 0;
	Command newCommand;
	while (1)
	{
        if (!draining) {
            newCommand = readCommand(traceFile);
            if (newCommand.type == 'E') draining = 1;
        }
        if (draining) { newCommand = addBubble(); strcpy(newCommand.command, "No Command"); }

		for (i = 4; i > 0; i--)
		{
			pipe->stage[i] = pipe->stage[i - 1];
		}
		pipe->stage[IF] = newCommand; //check memory allocation
		cycle++;
		printfCycleInfo(*pipe);
		if (pipe->forward == 0)	/*Forward defined as 0 branch (No forwarding)*/
		{
			if ((pipe->stage[EX].signals.RegWrite) && (pipe->stage[ID].operand2 == pipe->stage[EX].operand1 || pipe->stage[ID].operand3 == pipe->stage[EX].operand1))
			{
				stall(pipe, ID);	//example: [ID]=add $t0 $t1 $t2 , [EX]=sub $t1 $t2 $t3
				cycle++;
				printfCycleInfo(*pipe);
			}
			if ((pipe->stage[MEM].signals.RegWrite) && (pipe->stage[ID].operand2 == pipe->stage[MEM].operand1 || pipe->stage[ID].operand3 == pipe->stage[MEM].operand1))
			{
				stall(pipe, ID);	//example: [ID]=add $t0 $t1 $t2 , [MEM]=sub $t1 $t2 $t3
				cycle++;
				printfCycleInfo(*pipe);
			}
			if (pipe->branch == 0)	/*Branch in MEM stage*/
			{
				if ((pipe->stage[ID].signals.Branch) && pipe->stage[IF].type != '0' && pipe->stage[IF].address != pipe->stage[ID].address + 4)
				{
					flush1(pipe);
				}
			}
			else
			{
				if ((pipe->stage[ID].signals.Branch) && pipe->stage[IF].type != '0' && pipe->stage[IF].address != pipe->stage[ID].address + 4)
				{
					stall(pipe, ID);
					cycle++;
					printfCycleInfo(*pipe);
				}
			}
		}
		else
		{
			if ((pipe->stage[EX].signals.MemRead) && (pipe->stage[ID].operand2 == pipe->stage[EX].operand1 || pipe->stage[ID].operand3 == pipe->stage[EX].operand1))
			{
				stall(pipe, ID);	//example: [ID]=add $t0 $t1 $t2 , [EX]=lw $t1 10 $t3
				cycle++;
				printfCycleInfo(*pipe);
			}
			if (pipe->branch == 0)	/*Branch in MEM stage*/
			{
				if ((pipe->stage[ID].signals.Branch) && pipe->stage[IF].type != '0' && pipe->stage[IF].address != pipe->stage[ID].address + 4)
				{
					flush1(pipe);
				}
			}
			else         /*Branch in MEM stage*/
			{
				if ((pipe->stage[ID].signals.Branch) && pipe->stage[IF].type != '0' && pipe->stage[IF].address != pipe->stage[ID].address + 4)
				{
					stall(pipe, ID);
					cycle++;
					printfCycleInfo(*pipe);
				}
			}
		}
        if (draining && pipe->stage[IF].type == '0' && pipe->stage[ID].type == '0' &&
            pipe->stage[EX].type == '0' && pipe->stage[MEM].type == '0') return;
	}
}

void flush1(Pipeline* pipe)
{
	int i, j;
	for (j = 0; j < 2; j++)
	{
		for (i = 4; i > 1; i--)
		{
			pipe->stage[i] = pipe->stage[i - 1];
		}
		pipe->stage[i] = addBubble();			//check memory if works
		cycle++;
		//stalls++;
		printfCycleInfo(*pipe);
	}
}

void stall(Pipeline* pipe, int location)	//pipe[location] will be stalled
{
	int i;
	for (i = 4; i > location + 1; i--)
	{
		pipe->stage[i] = pipe->stage[i - 1];
	}
	pipe->stage[i] = addBubble();			//check memory if works
	//stalls++;
}

void flush(Pipeline* pipe)			//add 3 stalls
{
	int i, j;
	for (j = 0; j < 3; j++)
	{
		for (i = 4; i > 1; i--)
		{
			pipe->stage[i] = pipe->stage[i - 1];
		}
		pipe->stage[i] = addBubble();			//check memory if works
		cycle++;
		//stalls++;
		printfCycleInfo(*pipe);
	}
}

void bStall(Pipeline* pipe)	//pipe[location] will be stalled
{
	int i, j;
	for (j = 0; j < 3; j++)
	{
		for (i = 4; i > IF - 1; i--)
		{
			pipe->stage[i] = pipe->stage[i - 1];
		}
		pipe->stage[i] = addBubble();			//check memory if works
		cycle++;
		printfCycleInfo(*pipe);
	}
}


Command addBubble()	//returns a bubble/stall
{
	Command bubble = {0};
	bubble.type = '0';
	strcpy(bubble.name, "stll");
	strcpy(bubble.command, "stall");
	bubble.operand1 = 0;
	bubble.operand2 = 0;
	bubble.operand3 = 0;
	bubble.signals = stallControl;
	return bubble;
}

/*Assisting functions*/
char getCommandType(char* commandName)
{
	int i, j, t;
	for (i = 0; i < COMMANDS_NUM; i++)
	{
		t = 0;
		j = 0;
		do {
			if (commandName[t] != commands[i][j])
				break;
			t++;
			j++;
			if (commandName[t] == commands[i][j] && commandName[t] == '\0')
			{
				if (i > 11)
				{
					return 'J';
				}
				else if (i > 9)
				{
					return 'B';
				}
				else if (i > 8)
				{
					return 'S';
				}
				else if (i > 7)
				{
					return 'L';
				}
				else if (i > 3)
				{
					return 'I';
				}
				else return 'R';
			}
		} while (commandName[t]);
	}
	return '?';
}

int convertToBinary(int origin)
{
	int bin = 0, rem, t = 1;
	while (origin)
	{
		rem = origin % 2;
		bin = bin + rem * t;
		t *= 10;
		origin /= 2;
	}
	return bin;
}

long toBin(int dno)
{
	long bno = 0, remainder, f = 1;
	while (dno != 0)
	{
		remainder = dno % 2;
		bno = bno + remainder * f;
		f = f * 10;
		dno = dno / 2;
	}
	return bno;
}

void printCPI()
{
	if (instructions == 0) { printf("No instructions.\n"); return; }
	printf("Instructions: %d\nCycles: %d\nCPI: %.3f\n", instructions, (int)cycle, cycle / instructions);
}

void printfCycleInfo(Pipeline pipe)
{
	printf("Clock %d:\n", (int)cycle);
	printf("Fetch instruction: %s\n", pipe.stage[IF].command);
	printf("Decode instruction: %s\n", pipe.stage[ID].command);
	printf("Execute instruction: %s\n", pipe.stage[EX].command);
	printf("Memory instruction: %s\n", pipe.stage[MEM].command);
	printf("Writeback instruction: %s\n", pipe.stage[WB].command);
	if (!strcmp(pipe.stage[WB].command, "stall")) stalls++;
}

void endFile(Pipeline* pipe)
{
	int i, j;
	for (i = 0; i < 4; i++)
	{
		for (j = 4; j > 0; j--)
		{
			pipe->stage[j] = pipe->stage[j - 1];
		}
		pipe->stage[0] = addBubble();
		strcpy(pipe->stage[j].name, "nop");
		strcpy(pipe->stage[j].command, "No Command");
		cycle++;
		printfCycleInfo(*pipe);
	}
}

/*File functions*/
Command readCommand(FILE* filePtr)
{
    char line[256], mnemonic[16], extra[32];
    int address, r1, r2, value, matched;
    Command cmd = {0};
    do {
        if (!fgets(line, sizeof line, filePtr)) return endOfFile();
    } while (strspn(line, " \t\r\n") == strlen(line));
    if (!strchr(line, '\n') && !feof(filePtr)) {
        fprintf(stderr, "Trace line too long.\n"); parseError = 1; return endOfFile();
    }
    if (sscanf(line, "%d %15s", &address, mnemonic) != 2) {
        fprintf(stderr, "Invalid trace line: %s", line); parseError = 1; return endOfFile();
    }
    cmd.type = getCommandType(mnemonic);
    cmd.address = address;
    if (strlen(mnemonic) >= sizeof cmd.name) { parseError = 1; return endOfFile(); }
    strcpy(cmd.name, mnemonic);
    switch (cmd.type) {
    case 'R':
        matched = sscanf(line, "%*d %*s $%d $%d $%d %31s", &r1, &r2, &value, extra);
        if (matched != 3) break;
        cmd.signals = rControl; cmd.operand1 = r1; cmd.operand2 = r2; cmd.operand3 = value; goto valid;
    case 'I':
        matched = sscanf(line, "%*d %*s $%d $%d %d %31s", &r1, &r2, &value, extra);
        if (matched != 3) break;
        cmd.signals = iControl; cmd.operand1 = r1; cmd.operand2 = r2; cmd.operand3 = -1; goto valid;
    case 'L': case 'S':
        matched = sscanf(line, "%*d %*s $%d %d $%d %31s", &r1, &value, &r2, extra);
        if (matched != 3) break;
        cmd.signals = cmd.type == 'L' ? lControl : sControl;
        cmd.operand1 = cmd.type == 'L' ? r1 : -1;
        cmd.operand2 = r2; cmd.operand3 = cmd.type == 'S' ? r1 : -1; goto valid;
    case 'B':
        matched = sscanf(line, "%*d %*s $%d $%d %31s", &r1, &r2, extra);
        if (matched != 3) break;
        cmd.signals = bControl; cmd.operand1 = -1; cmd.operand2 = r1; cmd.operand3 = r2; goto valid;
    default: break;
    }
    fprintf(stderr, "Unsupported or invalid instruction: %s", line);
    parseError = 1; return endOfFile();
valid:
    if (r1 < 0 || r1 > 31 || r2 < 0 || r2 > 31 || (cmd.type == 'R' && (value < 0 || value > 31))) {
        fprintf(stderr, "Invalid register in: %s", line); parseError = 1; return endOfFile();
    }
    char *text = strchr(line, ' ');
    if (!text) { parseError = 1; return endOfFile(); }
    while (*text == ' ') text++;
    text[strcspn(text, "\r\n")] = '\0';
    if (strlen(text) >= sizeof cmd.command) { fprintf(stderr, "Instruction too long.\n"); parseError = 1; return endOfFile(); }
    strcpy(cmd.command, text);
    instructions++;
    return cmd;
}

Command endOfFile()
{
    Command end = addBubble();
    end.type = 'E';
    return end;
}

void initPipe(Pipeline* pipe)
{
	int i = 0;
	for (; i < 5; i++)
	{
		pipe->stage[i] = addBubble();
		strcpy(pipe->stage[i].name, "nop");
		strcpy(pipe->stage[i].command, "No Command");
	}
}
