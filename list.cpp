#include "rar.hpp"

static void ListFileHeader(FileHeader &hd,bool Verbose,bool Technical,bool &TitleShown);
static void ListFileAttr(uint A,int HostOS);
static void ListOldSubHeader(Archive &Arc);
static void ListNewSubHeader(CommandData *Cmd,Archive &Arc,bool Technical);

void ListArchive(CommandData *Cmd)
{
  Int64 SumPackSize=0,SumUnpSize=0;
  uint ArcCount=0,SumFileCount=0;
  bool Technical=(Cmd->Command[1]=='T');
  bool Verbose=(*Cmd->Command=='V');

  char ArcName[NM];
  wchar ArcNameW[NM];

  while (Cmd->GetArcName(ArcName,ArcNameW,sizeof(ArcName)))
  {
    Archive Arc(Cmd);
    if (!Arc.WOpen(ArcName,ArcNameW))
      continue;
    bool FileMatched=true;
    while (1)
    {
      Int64 TotalPackSize=0,TotalUnpSize=0;
      uint FileCount=0;
      if (Arc.IsArchive(true))
      {
        bool TitleShown=false;
        Arc.ViewComment();
//        Arc.SkipMhdExtra();
        mprintf("\n");
        if (Arc.Solid)
          mprintf(St(MListSolid));
        if (Arc.SFXSize>0)
          mprintf(St(MListSFX));
        if (Arc.Volume)
          if (Arc.Solid)
            mprintf(St(MListVol1));
          else
            mprintf(St(MListVol2));
        else
          if (Arc.Solid)
            mprintf(St(MListArc1));
          else
            mprintf(St(MListArc2));
        mprintf(" %s\n",Arc.FileName);
        if (Technical)
        {
          if (Arc.Protected)
            mprintf(St(MListRecRec));
          if (Arc.Locked)
            mprintf(St(MListLock));
        }
        while(Arc.ReadHeader()>0)
        {
          switch(Arc.GetHeaderType())
          {
            case FILE_HEAD:
              IntToExt(Arc.NewLhd.FileName,Arc.NewLhd.FileName);
              if ((FileMatched=Cmd->IsProcessFile(Arc.NewLhd))==true)
              {
                ListFileHeader(Arc.NewLhd,Verbose,Technical,TitleShown);
                if (!(Arc.NewLhd.Flags & LHD_SPLIT_BEFORE))
                {
                  TotalUnpSize+=Arc.NewLhd.FullUnpSize;
                  FileCount++;
                }
                TotalPackSize+=Arc.NewLhd.FullPackSize;
              }
              break;
#ifndef SFX_MODULE
            case SUB_HEAD:
              if (Technical && FileMatched)
                ListOldSubHeader(Arc);
              break;
#endif
            case NEWSUB_HEAD:
              if (FileMatched)
              {
                if (Technical)
                  ListFileHeader(Arc.SubHead,Verbose,true,TitleShown);
                ListNewSubHeader(Cmd,Arc,Technical);
              }
              break;
          }
          Arc.SeekToNext();
        }
        if (TitleShown)
        {
          mprintf("\n");
          for (int I=0;I<79;I++)
            mprintf("-");
          char UnpSizeText[20];
          itoa(TotalUnpSize,UnpSizeText);
      
          char PackSizeText[20];
          itoa(TotalPackSize,PackSizeText);
      
          mprintf("\n%5lu %16s %8s %3d%%\n",FileCount,UnpSizeText,
                  PackSizeText,ToPercent(TotalPackSize,TotalUnpSize));
          SumFileCount+=FileCount;
          SumUnpSize+=TotalUnpSize;
          SumPackSize+=TotalPackSize;
        }
        else
          mprintf(St(MListNoFiles));

        ArcCount++;

#ifndef NOVOLUME
        if (Cmd->VolSize!=0 && ((Arc.NewLhd.Flags & LHD_SPLIT_AFTER) ||
            Arc.GetHeaderType()==ENDARC_HEAD &&
            (Arc.EndArcHead.Flags & EARC_NEXT_VOLUME)!=0) &&
            MergeArchive(Arc,NULL,false,*Cmd->Command))
        {
          Arc.Seek(0,SEEK_SET);
        }
        else
#endif
          break;
      }
      else
      {
        if (Cmd->ArcNames->ItemsCount()<2)
          mprintf(St(MNotRAR),Arc.FileName);
        break;
      }
    }
  }
  if (ArcCount>1)
  {
    char UnpSizeText[20],PackSizeText[20];
    itoa(SumUnpSize,UnpSizeText);
    itoa(SumPackSize,PackSizeText);
    mprintf("\n%5lu %16s %8s %3d%%\n",SumFileCount,UnpSizeText,
            PackSizeText,ToPercent(SumPackSize,SumUnpSize));
  }
}


void ListFileHeader(FileHeader &hd,bool Verbose,bool Technical,bool &TitleShown)
{
  if (!TitleShown)
  {
    if (Verbose)
      mprintf(St(MListPathComm));
    else
      mprintf(St(MListName));
    mprintf(St(MListTitle));
    if (Technical)
      mprintf(St(MListTechTitle));
    for (int I=0;I<79;I++)
      mprintf("-");
    TitleShown=true;
  }

  if (hd.HeadType==NEWSUB_HEAD)
    mprintf(St(MSubHeadType),hd.FileName);

  mprintf("\n%c",(hd.Flags & LHD_PASSWORD) ? '*' : ' ');

  if (Verbose)
    mprintf("%s\n%12s ",hd.FileName,"");
  else
    mprintf("%-12s",PointToName(hd.FileName));

  char UnpSizeText[20],PackSizeText[20];
  itoa(hd.FullUnpSize,UnpSizeText);
  itoa(hd.FullPackSize,PackSizeText);

  mprintf(" %8s %8s ",UnpSizeText,PackSizeText);

  if ((hd.Flags & LHD_SPLIT_BEFORE) && (hd.Flags & LHD_SPLIT_AFTER))
    mprintf(" <->");
  else
    if (hd.Flags & LHD_SPLIT_BEFORE)
      mprintf(" <--");
    else
      if (hd.Flags & LHD_SPLIT_AFTER)
        mprintf(" -->");
      else
        mprintf("%3d%%",ToPercent(hd.FullPackSize,hd.FullUnpSize));

  char DateStr[50];
  ConvertDate(hd.FileTime,DateStr,false);
  mprintf(" %s ",DateStr);

  if (hd.HeadType==NEWSUB_HEAD)
    mprintf("  %c....B  ",(hd.SubFlags & SUBHEAD_FLAGS_INHERITED) ? 'I' : '.');
  else
    ListFileAttr(hd.FileAttr,hd.HostOS);

  mprintf(" %8.8lX",hd.FileCRC);
  mprintf(" m%d",hd.Method-0x30);
  if ((hd.Flags & LHD_WINDOWMASK)<=6*32)
    mprintf("%c",((hd.Flags&LHD_WINDOWMASK)>>5)+'a');
  else
    mprintf(" ");
  mprintf(" %d.%d",hd.UnpVer/10,hd.UnpVer%10);

  static char *RarOS[]={
    "DOS","OS/2","Win95/NT","Unix","MacOS","BeOS","","","",""
  };

  if (Technical)
    mprintf("\n%22s %8s %4s",RarOS[hd.HostOS],
            (hd.Flags & LHD_SOLID) ? St(MYes):St(MNo),
            (hd.Flags & LHD_VERSION) ? St(MYes):St(MNo));
}


void ListFileAttr(uint A,int HostOS)
{
  switch(HostOS)
  {
    case MS_DOS:
    case OS2:
    case WIN_32:
    case MAC_OS:
      mprintf("  %c%c%c%c%c%c  ",
              (A & 0x08) ? 'V' : '.',
              (A & 0x10) ? 'D' : '.',
              (A & 0x01) ? 'R' : '.',
              (A & 0x02) ? 'H' : '.',
              (A & 0x04) ? 'S' : '.',
              (A & 0x20) ? 'A' : '.');
      break;
    case UNIX:
    case BEOS:
      switch (A & 0xF000)
      {
        case 0x4000:
          mprintf("d");
          break;
        case 0xA000:
          mprintf("l");
          break;
        default:
          mprintf("-");
          break;
      }
      mprintf("%c%c%c%c%c%c%c%c%c",
              (A & 0x0100) ? 'r' : '-',
              (A & 0x0080) ? 'w' : '-',
              (A & 0x0040) ? ((A & 0x0800) ? 's':'x'):((A & 0x0800) ? 'S':'-'),
              (A & 0x0020) ? 'r' : '-',
              (A & 0x0010) ? 'w' : '-',
              (A & 0x0008) ? ((A & 0x0400) ? 's':'x'):((A & 0x0400) ? 'S':'-'),
              (A & 0x0004) ? 'r' : '-',
              (A & 0x0002) ? 'w' : '-',
              (A & 0x0001) ? 'x' : '-');
      break;
  }
}


#ifndef SFX_MODULE
void ListOldSubHeader(Archive &Arc)
{
  switch(Arc.SubBlockHead.SubType)
  {
    case EA_HEAD:
      mprintf(St(MListEAHead));
      break;
    case UO_HEAD:
      mprintf(St(MListUOHead),Arc.UOHead.OwnerName,Arc.UOHead.GroupName);
      break;
    case MAC_HEAD:
      mprintf(St(MListMACHead1),Arc.MACHead.fileType>>24,Arc.MACHead.fileType>>16,Arc.MACHead.fileType>>8,Arc.MACHead.fileType);
      mprintf(St(MListMACHead2),Arc.MACHead.fileCreator>>24,Arc.MACHead.fileCreator>>16,Arc.MACHead.fileCreator>>8,Arc.MACHead.fileCreator);
      break;
    case BEEA_HEAD:
      mprintf(St(MListBeEAHead));
      break;
    case NTACL_HEAD:
      mprintf(St(MListNTACLHead));
      break;
    case STREAM_HEAD:
      mprintf(St(MListStrmHead),Arc.StreamHead.StreamName);
      break;
    default:
      mprintf(St(MListUnkHead),Arc.SubBlockHead.SubType);
      break;
  }
}
#endif


void ListNewSubHeader(CommandData *Cmd,Archive &Arc,bool Technical)
{
  if (strcmp(Arc.SubHead.FileName,SUBHEAD_TYPE_CMT)==0 && !Cmd->DisableComment)
  {
    Array<byte> CmtData;
    int ReadSize=Arc.ReadCommentData(CmtData);
    if (ReadSize!=0)
    {
      mprintf(St(MFileComment));
      OutComment((char *)&CmtData[0],ReadSize);
    }
  }
}