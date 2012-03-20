/******************************************************************************
 *
 * Module Name: prutils - Preprocessor utilities
 *
 *****************************************************************************/

/*
 * Copyright (C) 2000 - 2012, Intel Corp.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification.
 * 2. Redistributions in binary form must reproduce at minimum a disclaimer
 *    substantially similar to the "NO WARRANTY" disclaimer below
 *    ("Disclaimer") and any redistribution must be conditioned upon
 *    including a substantially similar Disclaimer requirement for further
 *    binary redistribution.
 * 3. Neither the names of the above-listed copyright holders nor the names
 *    of any contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * Alternatively, this software may be distributed under the terms of the
 * GNU General Public License ("GPL") version 2 as published by the Free
 * Software Foundation.
 *
 * NO WARRANTY
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDERS OR CONTRIBUTORS BE LIABLE FOR SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGES.
 */

#include <contrib/dev/acpica/compiler/aslcompiler.h>
#include <contrib/dev/acpica/compiler/dtcompiler.h>


#define _COMPONENT          ASL_PREPROCESSOR
        ACPI_MODULE_NAME    ("prutils")


/*******************************************************************************
 *
 * FUNCTION:    PrSetLineNumber
 *
 * PARAMETERS:  OriginalLineNumber      - Line number in original source file,
 *                                        or include file
 *              PreprocessorLineNumber  - Line number in the preprocessed file
 *
 * RETURN:      None
 *
 * DESCRIPTION: Insert this mapping into the mapping data structure, for use
 *              in possible error/warning messages.
 *
 * Line number mapping functions.
 * For error messages, we need to keep track of the line number in the
 * original file, versus the preprocessed (.i) file.
 *
 ******************************************************************************/

void
PrSetLineNumber (
    UINT32                  OriginalLineNumber,
    UINT32                  PreprocessorLineNumber)
{
    UINT32                  Entry;
    PR_LINE_MAPPING         *Block;
    UINT32                  Index;
    UINT32                  i;


    Entry = PreprocessorLineNumber / PR_LINES_PER_BLOCK;
    Index = PreprocessorLineNumber % PR_LINES_PER_BLOCK;
    Block = Gbl_MapBlockHead;

    for (i = 0; i < Entry; i++)
    {
        /* Allocate new mapping blocks as necessary */

        if (!Block->Next)
        {
            Block->Next = UtLocalCalloc (sizeof (PR_LINE_MAPPING));
            Block->Next->Map = UtLocalCalloc (PR_LINES_PER_BLOCK * sizeof (UINT32));
        }

        Block = Block->Next;
    }

    Block->Map[Index] = OriginalLineNumber;
}


/*******************************************************************************
 *
 * FUNCTION:    PrGetLineNumber
 *
 * PARAMETERS:  PreprocessorLineNumber  - Line number in the preprocessed file
 *                                        (or, the "logical line number)
 *
 * RETURN:      The line number in the original source file or include file.
 *
 * DESCRIPTION: Return the mapped value of a line number in the preprocessed
 *              source file to the actual line number in the original source
 *              file.
 *
 ******************************************************************************/

UINT32
PrGetLineNumber (
    UINT32                  PreprocessorLineNumber)
{
    UINT32                  Entry;
    PR_LINE_MAPPING         *Block;
    UINT32                  Index;
    UINT32                  i;


    Entry = PreprocessorLineNumber / PR_LINES_PER_BLOCK;
    Index = PreprocessorLineNumber % PR_LINES_PER_BLOCK;
    Block = Gbl_MapBlockHead;

    for (i = 0; i < Entry; i++)
    {
        Block = Block->Next;
        if (!Block)
        {
            /* Bad error, should not happen */
            return (0);
        }
    }

    return (Block->Map[Index]);
}


/******************************************************************************
 *
 * FUNCTION:    PrGetNextToken
 *
 * PARAMETERS:  Buffer              - Current line buffer
 *              MatchString         - String with valid token delimiters
 *              Next                - Set to next possible token in buffer
 *
 * RETURN:      Next token (null-terminated). Modifies the input line.
 *              Remainder of line is stored in *Next.
 *
 * DESCRIPTION: Local implementation of strtok() with local storage for the
 *              next pointer. Not only thread-safe, but allows multiple
 *              parsing of substrings such as expressions.
 *
 *****************************************************************************/

char *
PrGetNextToken (
    char                    *Buffer,
    char                    *MatchString,
    char                    **Next)
{
    char                    *TokenStart;


    if (!Buffer)
    {
        /* Use Next if it is valid */

        Buffer = *Next;
        if (!(*Next))
        {
            return (NULL);
        }
    }

    /* Skip any leading delimiters */

    while (*Buffer)
    {
        if (strchr (MatchString, *Buffer))
        {
            Buffer++;
        }
        else
        {
            break;
        }
    }

    /* Anything left on the line? */

    if (!(*Buffer))
    {
        *Next = NULL;
        return (NULL);
    }

    TokenStart = Buffer;

    /* Find the end of this token */

    while (*Buffer)
    {
        if (strchr (MatchString, *Buffer))
        {
            *Buffer = 0;
            *Next = Buffer+1;
            if (!**Next)
            {
                *Next = NULL;
            }
            return (TokenStart);
        }
        Buffer++;
    }

    *Next = NULL;
    return (TokenStart);
}


/*******************************************************************************
 *
 * FUNCTION:    PrError
 *
 * PARAMETERS:  Level               - Seriousness (Warning/error, etc.)
 *              MessageId           - Index into global message buffer
 *              Column              - Column in current line
 *
 * RETURN:      None
 *
 * DESCRIPTION: Preprocessor error reporting. Front end to AslCommonError2
 *
 ******************************************************************************/

void
PrError (
    UINT8                   Level,
    UINT8                   MessageId,
    UINT32                  Column)
{
#if 0
    AcpiOsPrintf ("%s (%u) : %s", Gbl_Files[ASL_FILE_INPUT].Filename,
        Gbl_CurrentLineNumber, Gbl_CurrentLineBuffer);
#endif


    if (Column > 120)
    {
        Column = 0;
    }

    /* TBD: Need Logical line number? */

    AslCommonError2 (Level, MessageId,
        Gbl_CurrentLineNumber, Column,
        Gbl_CurrentLineBuffer,
        Gbl_Files[ASL_FILE_INPUT].Filename, "Preprocessor");

    Gbl_PreprocessorError = TRUE;
}


/*******************************************************************************
 *
 * FUNCTION:    PrReplaceData
 *
 * PARAMETERS:  Buffer              - Original(target) buffer pointer
 *              LengthToRemove      - Length to be removed from target buffer
 *              BufferToAdd         - Data to be inserted into target buffer
 *              LengthToAdd         - Length of BufferToAdd
 *
 * RETURN:      None
 *
 * DESCRIPTION: Generic buffer data replacement.
 *
 ******************************************************************************/

void
PrReplaceData (
    char                    *Buffer,
    UINT32                  LengthToRemove,
    char                    *BufferToAdd,
    UINT32                  LengthToAdd)
{
    UINT32                  BufferLength;


    /* Buffer is a string, so the length must include the terminating zero */

    BufferLength = strlen (Buffer) + 1;

    if (LengthToRemove != LengthToAdd)
    {
        /*
         * Move some of the existing data
         * 1) If adding more bytes than removing, make room for the new data
         * 2) if removing more bytes than adding, delete the extra space
         */
        if (LengthToRemove > 0)
        {
            memmove ((Buffer + LengthToAdd), (Buffer + LengthToRemove),
                (BufferLength - LengthToRemove));
        }
    }

    /* Now we can move in the new data */

    if (LengthToAdd > 0)
    {
        memmove (Buffer, BufferToAdd, LengthToAdd);
    }
}


/*******************************************************************************
 *
 * FUNCTION:    PrOpenIncludeFile
 *
 * PARAMETERS:  Filename            - Filename or pathname for include file
 *
 * RETURN:      None.
 *
 * DESCRIPTION: Open an include file and push it on the input file stack.
 *
 ******************************************************************************/

void
PrOpenIncludeFile (
    char                    *Filename)
{
    FILE                    *IncludeFile;
    ASL_INCLUDE_DIR         *NextDir;


    /*
     * start the actual include file on the next line
     */
    Gbl_CurrentLineOffset++;

    /* Attempt to open the include file */

    /* If the file specifies an absolute path, just open it */

    if ((Filename[0] == '/')  ||
        (Filename[0] == '\\') ||
        (Filename[1] == ':'))
    {
        IncludeFile = PrOpenIncludeWithPrefix ("", Filename);
        if (!IncludeFile)
        {
            goto ErrorExit;
        }
        return;
    }

    /*
     * The include filename is not an absolute path.
     *
     * First, search for the file within the "local" directory -- meaning
     * the same directory that contains the source file.
     *
     * Construct the file pathname from the global directory name.
     */
    IncludeFile = PrOpenIncludeWithPrefix (Gbl_DirectoryPath, Filename);
    if (IncludeFile)
    {
        return;
    }

    /*
     * Second, search for the file within the (possibly multiple)
     * directories specified by the -I option on the command line.
     */
    NextDir = Gbl_IncludeDirList;
    while (NextDir)
    {
        IncludeFile = PrOpenIncludeWithPrefix (NextDir->Dir, Filename);
        if (IncludeFile)
        {
            return;
        }

        NextDir = NextDir->Next;
    }

    /* We could not open the include file after trying very hard */

ErrorExit:
    sprintf (Gbl_MainTokenBuffer, "%s, %s", Filename, strerror (errno));
    PrError (ASL_ERROR, ASL_MSG_INCLUDE_FILE_OPEN, 0);
}


/*******************************************************************************
 *
 * FUNCTION:    FlOpenIncludeWithPrefix
 *
 * PARAMETERS:  PrefixDir       - Prefix directory pathname. Can be a zero
 *                                length string.
 *              Filename        - The include filename from the source ASL.
 *
 * RETURN:      Valid file descriptor if successful. Null otherwise.
 *
 * DESCRIPTION: Open an include file and push it on the input file stack.
 *
 ******************************************************************************/

FILE *
PrOpenIncludeWithPrefix (
    char                    *PrefixDir,
    char                    *Filename)
{
    FILE                    *IncludeFile;
    char                    *Pathname;


    /* Build the full pathname to the file */

    Pathname = ACPI_ALLOCATE (strlen (PrefixDir) + strlen (Filename) + 1);

    strcpy (Pathname, PrefixDir);
    strcat (Pathname, Filename);

    DbgPrint (ASL_PARSE_OUTPUT, "\n" PR_PREFIX_ID
        "Opening include file: path %s\n",
        Gbl_CurrentLineNumber, Pathname);

    /* Attempt to open the file, push if successful */

    IncludeFile = fopen (Pathname, "r");
    if (IncludeFile)
    {
        /* Push the include file on the open input file stack */

        PrPushInputFileStack (IncludeFile, Pathname);
        return (IncludeFile);
    }

    ACPI_FREE (Pathname);
    return (NULL);
}


/*******************************************************************************
 *
 * FUNCTION:    AslPushInputFileStack
 *
 * PARAMETERS:  InputFile           - Open file pointer
 *              Filename            - Name of the file
 *
 * RETURN:      None
 *
 * DESCRIPTION: Push the InputFile onto the file stack, and point the parser
 *              to this file. Called when an include file is successfully
 *              opened.
 *
 ******************************************************************************/

void
PrPushInputFileStack (
    FILE                    *InputFile,
    char                    *Filename)
{
    PR_FILE_NODE            *Fnode;


    /* Save the current state in an Fnode */

    Fnode = UtLocalCalloc (sizeof (PR_FILE_NODE));

    Fnode->File = Gbl_Files[ASL_FILE_INPUT].Handle;
    Fnode->Next = Gbl_InputFileList;
    Fnode->Filename = Gbl_Files[ASL_FILE_INPUT].Filename;
    Fnode->CurrentLineNumber = Gbl_CurrentLineNumber;

    /* Push it on the stack */

    Gbl_InputFileList = Fnode;

    DbgPrint (ASL_PARSE_OUTPUT, PR_PREFIX_ID
        "Push InputFile Stack, returning %p\n\n",
        Gbl_CurrentLineNumber, InputFile);

    /* Reset the global line count and filename */

    Gbl_Files[ASL_FILE_INPUT].Filename = Filename;
    Gbl_Files[ASL_FILE_INPUT].Handle = InputFile;
    Gbl_CurrentLineNumber = 1;
}


/*******************************************************************************
 *
 * FUNCTION:    AslPopInputFileStack
 *
 * PARAMETERS:  None
 *
 * RETURN:      0 if a node was popped, -1 otherwise
 *
 * DESCRIPTION: Pop the top of the input file stack and point the parser to
 *              the saved parse buffer contained in the fnode.  Also, set the
 *              global line counters to the saved values.  This function is
 *              called when an include file reaches EOF.
 *
 ******************************************************************************/

BOOLEAN
PrPopInputFileStack (
    void)
{
    PR_FILE_NODE            *Fnode;


    Fnode = Gbl_InputFileList;
    DbgPrint (ASL_PARSE_OUTPUT, "\n" PR_PREFIX_ID
        "Pop InputFile Stack, Fnode %p\n\n",
        Gbl_CurrentLineNumber, Fnode);

    if (!Fnode)
    {
        return (FALSE);
    }

    /* Close the current include file */

    fclose (Gbl_Files[ASL_FILE_INPUT].Handle);

    /* Update the top-of-stack */

    Gbl_InputFileList = Fnode->Next;

    /* Reset global line counter and filename */

    Gbl_Files[ASL_FILE_INPUT].Filename = Fnode->Filename;
    Gbl_Files[ASL_FILE_INPUT].Handle = Fnode->File;
    Gbl_CurrentLineNumber = Fnode->CurrentLineNumber;

    /* All done with this node */

    ACPI_FREE (Fnode);
    return (TRUE);
}
