#include "mumsh.h"
// void cHandler(){
//     // signal(SIGINT, cHandler);
//     debugMsg("Info: sending Ctrl-C\n");
//     if(nodeStatus == PARENT_NORMAL) {
//         debugMsg("Info: Node Status-Parent Normal.\n");
//         nodeStatus = PARENT_EXIT;
//     }
//     else if(nodeStatus == CHILD_NORMAL){
//         debugMsg("Info: Node Status-Child Normal.\n");
//         exit(0);
//     }
// }
struct sigaction old_action;
struct sigaction action;
void sigint_handler()
{
    sigaction(SIGINT, &old_action, NULL);
    debugMsg("Info: sending Ctrl-C\n");
    if(nodeStatus == PARENT_NORMAL) {
        debugMsg("Info: Node Status-Parent Normal.\n");
        nodeStatus = PARENT_EXIT;
    }
    else if(nodeStatus == CHILD_NORMAL){
        debugMsg("Info: Node Status-Child Normal.\n");
        exit(0);
    }
}

int main(){
    // signal(SIGINT, cHandler);
    action.sa_handler = &sigint_handler;
    bgCnt = 0;
    for(int i=0;i<MAX_BGPROC;i++) bgCommand[i]=NULL;
    while(1){
        sigaction(SIGINT, &action, &old_action);
        nodeStatus = PARENT_NORMAL;
        prompt("mumsh $ ");
        
        /** Initialization
         * @isInRed, isOutRed, isOutApp: redirection status
         */
        int isInRed=0, isOutRed=0, isOutApp=0; // redirection status
        int desIn, desOut; // file descriptor
        int pipedInitAddr[MAX_PIPED];
        memset(pipedInitAddr,0,MAX_PIPED);
        promptInit();

        /** Input
         * @line: original line of commands
         * @conjLine: pre-processed line of commands
         */
        int isInputNotEnd = 0, isFirstfgets = 1;
        int isSQuoNotClosed = 0, isDQuoNotClosed = 0;
        while(isInputNotEnd || isFirstfgets){
            isInputNotEnd = 0;
            if(isFirstfgets) isFirstfgets = 0;
            else prompt("> ");
            if(fgets(line, MAX_LINE, stdin) == NULL){
                free(line);
                free(conjLine);
                promptExit();
                debugMsg("Info: fgets quit.\n");
                if(feof(stdin)) continue;
                if(errno == EINTR) continue; // fgets meet SIGINT
                stdoutMsg("exit\n");
                exit(0);
            }
            
            /* check incomplete quotation mark and update the flag */
            for(unsigned int i=0;i<strlen(line);i++){
                if(line[i]=='\''){
                    if(!isSQuoNotClosed && !isDQuoNotClosed) isSQuoNotClosed = 1;
                    else if(isSQuoNotClosed) isSQuoNotClosed = 0;
                }
                else if(line[i]=='\"'){
                    if(!isSQuoNotClosed && !isDQuoNotClosed) isDQuoNotClosed = 1;
                    else if(isDQuoNotClosed && (i==0 || (i>0 && line[i-1]!='\\'))) isDQuoNotClosed = 0;
                }
            }
            if(isSQuoNotClosed || isDQuoNotClosed){
                strcat(conjLine, line);
                isInputNotEnd = 1;
                continue;
            }
            for(unsigned int i=strlen(line)-2;i>=0;i--){ // assume that the last character is newline
                if(line[i]==' ') continue;
                else if(line[i]=='>' || line[i]=='<' || line[i]=='|'){
                    isInputNotEnd = 1;
                    break;
                }
                else break;
            }
            if(isInputNotEnd){
                line[strlen(line)-1] = ' '; // replace newline with blank
                strcat(conjLine, line);
                continue;
            }
            /* concatenation */
            strcat(conjLine, line);
            isInputNotEnd = 0; // break;
        }
        free(line);
        fflush(stdin);
        if(nodeStatus==PARENT_EXIT){ // no longer necessary
            free(conjLine);
            promptExit();
            continue;
        }

        /** Parsing conjLine
         * EFFECTS: validate whether the input is emtpy
         */
        int isEmptyLine=1;
        for(unsigned int i=0;i<strlen(conjLine);i++){
            if(conjLine[i]!=32 && conjLine[i]!=10) isEmptyLine=0;
        }
        if(isEmptyLine){
            debugMsg("Empty line.\n");
            free(conjLine);
            promptExit();
            continue;
        }

        /** Parsing background character
         */
        if(conjLine[strlen(conjLine)-2] == '&'){
            isBackground = 1;
            bgCommand[bgCnt] = (char *)malloc(sizeof(char)*MAX_LINE);
            memset(bgCommand[bgCnt],0,MAX_LINE);
            strcpy(bgCommand[bgCnt], conjLine);
            bgCommand[bgCnt][strlen(bgCommand[bgCnt])-1] = '\0';
        }

        /** Parsing quotation mark
         * 
         */
        isSQuoNotClosed = 0;
        isDQuoNotClosed = 0;
        int specialCnt = 0;
        char specialList[MAX_LINE];
        memset(specialList,0,MAX_LINE);
        unsigned int deleteCnt = 0;
        unsigned int deleteList[MAX_LINE];
        for(unsigned int i=0;i<strlen(conjLine)-1;i++){ // omitted the newline in the end
            if(isSQuoNotClosed || isDQuoNotClosed){
                if(conjLine[i]=='>') {conjLine[i]=Q_REPLACER;specialList[specialCnt++]='>';}
                if(conjLine[i]=='<') {conjLine[i]=Q_REPLACER;specialList[specialCnt++]='<';}
                if(conjLine[i]=='|') {conjLine[i]=Q_REPLACER;specialList[specialCnt++]='|';}
                if(conjLine[i]==' ') {conjLine[i]=Q_REPLACER;specialList[specialCnt++]=' ';}
                if(conjLine[i]=='\n') {conjLine[i]=Q_REPLACER;specialList[specialCnt++]='\n';}
            }
            if(conjLine[i]=='\''){
                if(!isSQuoNotClosed && !isDQuoNotClosed){
                    deleteList[deleteCnt++] = i; // DEL '
                    isSQuoNotClosed = 1;
                }
                else if(isSQuoNotClosed){
                    deleteList[deleteCnt++] = i; // DEL '
                    isSQuoNotClosed = 0;
                }
            }
            else if(conjLine[i]=='\"'){
                if(!isSQuoNotClosed && !isDQuoNotClosed){
                    deleteList[deleteCnt++] = i; // DEL "
                    isDQuoNotClosed = 1;
                }
                else if(isDQuoNotClosed && i>0 && conjLine[i-1]=='\\'){
                    deleteList[deleteCnt++] = i; // DEL slash
                }
                else if(isDQuoNotClosed && (i==0 || (i>0 && conjLine[i-1]!='\\'))){
                    deleteList[deleteCnt++] = i; // DEL "
                    isDQuoNotClosed = 0;
                }
            }
        }
        char tmpLine[MAX_LINE];
        memset(tmpLine,0,MAX_LINE);
        for(unsigned int i=0,j=0,k=0;i<strlen(conjLine);i++){
            if(j>=deleteCnt || (j<deleteCnt && i!=deleteList[j])) tmpLine[k++]=conjLine[i];
            else if(j<deleteCnt && i==deleteList[j]) j++;
        }
        memset(conjLine,0,MAX_LINE);
        strcpy(conjLine,tmpLine);
        /** Parsing redirection
         * @Sline: conjLine with keywords >, <, >> separated by space
         */
        char *sLine = (char *)malloc(sizeof(char)*MAX_LINE*2);
        memset(sLine,0,MAX_LINE*2);
        for(unsigned int i=0,j=0;i<strlen(conjLine);){
            if(conjLine[i]!='<' && conjLine[i]!='>' && conjLine[i]!='|') sLine[j++]=conjLine[i++];
            else{
                sLine[j++]=' ';
                sLine[j++]=conjLine[i++];
                if(i<strlen(conjLine)&&conjLine[i]=='>') sLine[j++]=conjLine[i++];
                if(i<strlen(conjLine)&&conjLine[i]!=' ') sLine[j++]=' ';
            }
        }
        free(conjLine);

        // TODO: check whether there is only blank characters in the line (?)
        // TODO: check whether the pipe is valid, error handling here.

        /** Tokenize the line; parsing redirection symbols
         * @mArgv: arguments from the input line
         * @mArgc: argument count in the input line
         */
        char **mArgv = (char **)malloc(sizeof(char *)*MAX_LINE); // arguments from the input line
        for(int i=0;i<MAX_LINE;i++) mArgv[i]=NULL; // init the parameter array
        int mArgc = 0; // argument count in the input line
        char *token;
        token = strtok(sLine, PARM_DELIM);
        while (token != NULL){
            // original redirection
            if (token[0]=='>'){
                if(strlen(token)>1 && token[1]=='>'){
                    isOutApp=1;
                    if(strlen(token)==2){
                        token = strtok(NULL, PARM_DELIM);
                        memset(outFileName, 0, MAX_FILENAME);
                        strcpy(outFileName, token);
                        token = strtok(NULL, PARM_DELIM);
                        continue;
                    }
                    else{
                        memset(outFileName, 0, MAX_FILENAME);
                        strcpy(outFileName, token);
                        memmove(outFileName, outFileName+2, strlen(outFileName));
                        token = strtok(NULL, PARM_DELIM);
                        continue;
                    }
                }
                else{
                    isOutRed=1;
                    if(strlen(token)==1){
                        token = strtok(NULL, PARM_DELIM);
                        memset(outFileName, 0, MAX_FILENAME);
                        strcpy(outFileName, token);
                        token = strtok(NULL, PARM_DELIM);
                        continue;
                    }
                    else{
                        memset(outFileName, 0, MAX_FILENAME);
                        strcpy(outFileName, token);
                        memmove(outFileName, outFileName+1, strlen(outFileName));
                        token = strtok(NULL, PARM_DELIM);
                        continue;
                    }
                }
            }
            else if (token[0]=='<'){
                isInRed=1;
                if(strlen(token)==1){
                    token = strtok(NULL, PARM_DELIM); // TODO: Error: emtpy inFileName
                    memset(inFileName, 0, MAX_FILENAME);
                    strcpy(inFileName, token);
                    token = strtok(NULL, PARM_DELIM);
                    continue;
                }
                else{
                    memset(inFileName, 0, MAX_FILENAME);
                    strcpy(inFileName, token);
                    memmove(inFileName, inFileName+1, strlen(inFileName));
                    token = strtok(NULL, PARM_DELIM);
                    continue;
                }
            }

            mArgv[mArgc] = (char *)malloc(sizeof(char)*(strlen(token)+1));
            memset(mArgv[mArgc], 0, strlen(token)+1); // init parameter
            strcpy(mArgv[mArgc], token);
            mArgc++;
            token = strtok(NULL, PARM_DELIM);
        }
        if(isInRed) debugMsg("inred\n");
        if(isOutApp) debugMsg("outapp\n");
        if(isOutRed) debugMsg("outred\n");

        /** Parsing pipe
         * @pipeCnt: number of pipes in the line
         * @cmdHeadDict: location dictionary of command heads
         * @cmdCnt: number of commands in the line
         */
        int pipeCnt=0; // number of pipes in the line
        int cmdHeadDict[MAX_PIPED]; // location dictionary of command heads
        cmdHeadDict[0]=0;
        for(int i=0,j=1;i<mArgc;i++){
            if(!strcmp(mArgv[i], "|")){
                pipeCnt++;
                free(mArgv[i]);
                mArgv[i]=NULL;
                cmdHeadDict[j++]=i+1;
            }
            else if(!strcmp(mArgv[i], "&")) {free(mArgv[i]); mArgv[i]=NULL;}
        }
        int cmdCnt = pipeCnt + 1; // number of commands in the line
        cmdHeadDict[cmdCnt] = mArgc + 1; // for the purpose of calculating offset
        
        /** Recreating special characters
         */ 
        int spIndex = 0;
        for(int i=0;i<mArgc;i++){
            if(mArgv[i]==NULL) continue;
            for(unsigned int j=0;j<strlen(mArgv[i]);j++){
                if(mArgv[i][j] == Q_REPLACER && spIndex<specialCnt) mArgv[i][j] = specialList[spIndex++];
            }
        }

        /** Creating pipe
         * @pipeFd: file descriptor for read/write ends of pipes
         */
        int pipeFd[pipeCnt*2+2]; // file descriptor for read/write ends of pipes
        for(int i=0;i<pipeCnt;i++){
            if(pipe(pipeFd + i*2) < 0){
                errMsg("Error: pipe failure.\n");
                exit(0);
            }
        }

        /** Executing commands
         * @childStatus
         */
        int childStatus;
        for(int index=0;index<cmdCnt;index++){
            int cmdHead = cmdHeadDict[index];
            int cmdOffset = cmdHeadDict[index+1] - cmdHead - 1;
            /* checking exit */
            if(!strcmp(mArgv[cmdHead],"exit")){
                stdoutMsg("exit\n");
                for(int i=0;i<mArgc;i++) if(mArgv[i]!=NULL) free(mArgv[i]);
                free(mArgv);
                free(sLine);
                promptExit();
                exit(0);
            }
            /* checking build-in*/
            if(!strcmp(mArgv[cmdHead],"cd")){ // FIXME: pipe, location, syntax
                // const char *dir = "~";
                if(cmdOffset==1){
                    if(chdir("/.") < 0){ // TODO: "~" need to be replaced by a specific directory (related with the user) (also without any other parameters)
                        // TODO: "cd -" need to be special judged (store the last result)
                        errMsg("Error: cd ~ not working.\n");
                    }
                }
                else{
                    // TO-CHECK: do we need to support arguments? more than one?
                    if(chdir(mArgv[cmdHead+1]) < 0){
                        errMsg("Error: cd not working.\n");
                    }
                    // printf("%s\n",mArgv[cmdHead+1]);fflush(stdout);
                }
                continue;
            }
            if(!strcmp(mArgv[cmdHead],"jobs")){
                // if(bgCnt==0)stdoutMsg("\n");
                for(int i=0;i<bgCnt;i++){
                    char tmpMsg[MAX_LINE];
                    if(waitpid(bgJob[i*2],NULL,WNOHANG)==0) sprintf(tmpMsg,"[%d] running %s\n",i+1,bgCommand[i]);
                    else sprintf(tmpMsg,"[%d] done %s\n",i+1,bgCommand[i]);
                    stdoutMsg(tmpMsg);
                }
                continue;
            }

            /* forking */
            pid_t pid = fork(); // TODO: check cd running in background
            if(pid > 0 && isBackground==1 && index==0){
                bgJob[bgCnt*2] = pid;
                bgJob[bgCnt*2+1] = PROC_RUNNING;
                bgCnt++;
                char tmpMsg[MAX_LINE];
                sprintf(tmpMsg,"[%d] %s\n",bgCnt,bgCommand[bgCnt-1]);
                stdoutMsg(tmpMsg);
            }
            
            if(pid < 0){ // fork error
                errMsg("Error: fork failed.\n");
                exit(0);
            }
            else if (pid == 0){ // child process
                nodeStatus = CHILD_NORMAL;
                /* connecting child pipeFd */
                if(index+1 < cmdCnt){ // not the last command
                    if(pipeCnt>0 && dup2(pipeFd[index*2+1], 1) < 0){
                        errMsg("Error: dup2-stdout failure.\n");
                        exit(0);
                    }
                }
                if(index!=0){ // not the first command
                    if(pipeCnt>0 && dup2(pipeFd[index*2-2], 0) < 0){
                        errMsg("Error: pip2-stdin failure.\n");
                        exit(0);
                    }
                }
                /* checking redirection */
                if(index==0 && isInRed){
                    desIn = open(inFileName,O_RDONLY); // TODO: check the parameters
                    dup2(desIn, 0); // replace stdin(0) with desIn
                    close(desIn);
                }
                if(index+1==cmdCnt && isOutRed){
                    desOut = open(outFileName, O_WRONLY | O_CREAT | O_TRUNC, S_IRWXU);
                    dup2(desOut, 1); // replace stdout(1) with desOut
                    close(desOut);
                }
                if(index+1==cmdCnt && isOutApp){
                    desOut = open(outFileName, O_WRONLY | O_CREAT | O_APPEND, S_IRWXU);
                    dup2(desOut, 1);
                    close(desOut);
                }
                /* closing child pipeFd */
                for(int i=0;i<2*pipeCnt;i++){
                    close(pipeFd[i]);
                }
                /* running build-in */
                if(!strcmp(mArgv[cmdHead],"pwd")){
                    char cwd[MAX_PATH];
                    if(getcwd(cwd, sizeof(cwd)) != NULL){
                        debugMsg("Info: pwd working.\n");
                        stdoutMsg(cwd);
                        stdoutMsg("\n");
                    }
                    else{
                        errMsg("Error: pwd not working.\n");
                    }
                    exit(0);
                }
                
                /* running bash command */
                if(execvp(mArgv[cmdHead], mArgv+cmdHead) < 0){
                    errMsg("Error: execvp not working.\n");
                    // exit(0);
                }
                // a successful call to execvp doesn't have a return value, so code after this line will not be reached.
                exit(0);
            }
            else{ // parent process
                
            }
        }
        for(int i=0;i<2*pipeCnt;i++){
            close(pipeFd[i]); // parent closing pipes
        }
        if(isBackground == 0){
            for(int i=0;i<pipeCnt+1;i++){
            wait(&childStatus); // parent waiting for child process
            char tmpMsg[108];
            sprintf(tmpMsg,"Child process status: %d\n",childStatus);
            debugMsg(tmpMsg);
            /* pid_t tmpPid;
            do{
                tmpPid = wait(&childStatus);
                if(tmpPid != pid){
                    char tmpMsg[1024];
                    sprintf(tmpMsg, "Error: The background process [%d] need to be terminated!\n", (int)tmpPid);
                    errMsg(tmpMsg); // background process
                }
            } while (tmpPid != pid); */
            }
        }
        else{ // The process is running in the background
            waitpid(bgJob[(bgCnt-1)*2],&childStatus,WNOHANG);
        }
        

        for(int i=0;i<mArgc;i++) if(mArgv[i]!=NULL) free(mArgv[i]);
        free(mArgv);
        free(sLine);
        promptExit();
    }
    for(int i=0;i<bgCnt;i++) free(bgCommand[i]);
    return 0;
}
