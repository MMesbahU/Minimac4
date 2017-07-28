#include "HaplotypeSet.h"

#include "assert.h"


#include "STLUtilities.h"

void HaplotypeSet::UncompressTypedSitesNew(HaplotypeSet &rHap,HaplotypeSet &tHap,int ChunkNo)
{
//    outFile=MyAllVariables->myOutFormat.OutPrefix;
    vcfType=false;

    int CPU=1;

    int    blockSize = 500;
    int    bufferSize = 5000;

    vector<CompressedHaplotype> CompHaplotypes(CPU);

    int currentPiece=0;
    for(currentPiece=0;currentPiece<CPU;currentPiece++)
    {
        CompHaplotypes[currentPiece].Size(rHap.ReducedStructureInfo, bufferSize);

    }
    currentPiece=0;

    vector<int> BufferPosList(CPU);
    BufferPosList[0]=1;
    for(int BufferNo=1;BufferNo<CPU;BufferNo++)
    {
        BufferPosList[BufferNo]= BufferPosList[BufferNo-1]+(bufferSize-1);
    }
    ReducedStructureInfoBuffer.clear();
    ReducedStructureInfoBuffer.resize(CPU);
    ReducedStructureInfo.clear();


    int NoMarkersWritten=1;
    int RefmarkerIndex=-1;
//    int CompressedIndex=0;
    vector<ReducedHaplotypeInfo> &RhapInfo=rHap.ReducedStructureInfo;


//    assert(numMarkers==tHap.numMarkers);

    for(int ThisPanelIndex=0; ThisPanelIndex<numMarkers; ThisPanelIndex++)
    {

        RefmarkerIndex=rHap.MapTarToRef[ThisPanelIndex];
//        assert(RefmarkerIndex<rHap.numMarkers);

        int j=rHap.MarkerToReducedInfoMapper[RefmarkerIndex];
//        assert(j<rHap.NoBlocks);

        ReducedHaplotypeInfo &ThisRhapInfo=RhapInfo[j];

        int ThisMarkerIndex=RefmarkerIndex-ThisRhapInfo.startIndex;
//        assert(ThisMarkerIndex<ThisRhapInfo.BlockSize);

        AlleleFreq[ThisPanelIndex]=rHap.AlleleFreq[RefmarkerIndex];
        if(MyAllVariables->myOutFormat.verbose)
        {
            VariantList[ThisPanelIndex]=rHap.VariantList[RefmarkerIndex];
        }
        CompHaplotypes[currentPiece].Push(j,ThisMarkerIndex);


            int NewPiece=CPU;
            if (CompHaplotypes[currentPiece].Length == bufferSize)
            {
                NewPiece=(currentPiece+1)%CPU;

                vector<String> tempHaplotypes(numHaplotypes);
                if(NewPiece!=0)
                {

                    int temp=CompHaplotypes[currentPiece].ReducedInfoMapper[bufferSize-1];
                    int temp2=CompHaplotypes[currentPiece].ReducedInfoVariantMapper[bufferSize-1];
                    CompHaplotypes[NewPiece].Clear();
                    CompHaplotypes[NewPiece].Push(temp,temp2);
                    currentPiece=NewPiece;
                }
            }


        if(NewPiece==0 || ThisPanelIndex==(numMarkers-1))
        {

            #pragma omp parallel for
            for(int ThisPiece=0;ThisPiece<=currentPiece;ThisPiece++)
            {

                int LastflushPos=BufferPosList[ThisPiece]-1;
                vector<int> index(numHaplotypes),oldIndex;
                vector<int> previousDifference(numHaplotypes);
                vector<int> previousPredecessor(numHaplotypes);
                vector<int> firstDifference(numHaplotypes-1,0);
                vector<int> cost(bufferSize+1,0);
                vector<int> bestSlice(bufferSize+1,0);
                vector<int> bestComplexity(bufferSize+1,0);
                vector<vector<int> > bestIndex(bufferSize+1);

                ReducedStructureInfoBuffer[ThisPiece].clear();
                findUnique RefUnique;
                RefUnique.updateCoeffs(MyAllVariables->myModelVariables.transFactor,MyAllVariables->myModelVariables.cisFactor);
                double blockedCost = 0.0;

                for(int i=0;i<numHaplotypes;i++)
                    index[i]=i;

                for(int length=1;length<=CompHaplotypes[currentPiece].Length;length++)
                {

                    CompHaplotypes[ThisPiece].RetrieveVariant(length-1);

                    vector<int> offsets(3,0);
                    for (int i = 0; i < numHaplotypes; i++)
                    {
                        offsets[CompHaplotypes[ThisPiece].GetVal(i) - '0' + 1]++;
                    }

                    offsets[2]+=offsets[1];
                    oldIndex = index;
                    for (int i = 0; i < numHaplotypes; i++)
                    {
                        index[offsets[CompHaplotypes[ThisPiece].GetVal(oldIndex[i],length - 1) - '0']++] = oldIndex[i];
                    }

                    RefUnique.UpdateDeltaMatrix(CompHaplotypes[ThisPiece], index, firstDifference, length, blockSize,
                           oldIndex, previousPredecessor, previousDifference);

                    RefUnique.AnalyzeBlocks(index, firstDifference, length, blockSize,
                       cost, bestSlice, bestComplexity, bestIndex);

                }

                if(CompHaplotypes[ThisPiece].Length>1)
                    blockedCost += RefUnique.FlushBlocks(ReducedStructureInfoBuffer[ThisPiece],
                                                        LastflushPos,
                                                        CompHaplotypes[ThisPiece], cost,
                                                        bestComplexity, bestSlice, bestIndex);

            }




            NoMarkersWritten+=(CPU*(bufferSize-1));

            BufferPosList[0]=NoMarkersWritten;
            for(int BufferNo=1;BufferNo<CPU;BufferNo++)
            {
                BufferPosList[BufferNo]= BufferPosList[BufferNo-1]+(bufferSize-1);

            }

            int temp=CompHaplotypes[currentPiece].ReducedInfoMapper[bufferSize-1];
            int temp2=CompHaplotypes[currentPiece].ReducedInfoVariantMapper[bufferSize-1];

            CompHaplotypes[0].Clear();
            CompHaplotypes[0].Push(temp,temp2);


            for(int ThisPiece=0;ThisPiece<CPU;ThisPiece++)
            {
                for(int jj=0;jj<(int)ReducedStructureInfoBuffer[ThisPiece].size();jj++)
                    {
                        ReducedStructureInfo.push_back(ReducedStructureInfoBuffer[ThisPiece][jj]);
                    }
                ReducedStructureInfoBuffer[ThisPiece].clear();
            }
            currentPiece=0;
        }

    }

    CreateAfterUncompressSummary();
    CreateScaffoldedParameters(rHap);
    InvertUniqueIndexMap();

    if(MyAllVariables->myOutFormat.verbose)
    {
        stringstream strs;
        strs<<(ChunkNo+1);
        string ss=(string)MyAllVariables->myOutFormat.OutPrefix+".chunk."+(string)(strs.str())+".GWAS";
        String tempString(ss.c_str());

        writem3vcfFile( tempString, MyAllVariables->myOutFormat.gzip);
    }


}

void HaplotypeSet::CreateScaffoldedParameters(HaplotypeSet &rHap)
{

    for(int i=0;i<numMarkers;i++)
    {
        Error[i]=rHap.Error[rHap.MapTarToRef[i]];
        if(i>0)
        {
            int index=rHap.MapTarToRef[i-1];
            double temp=1.0;
            while(index<rHap.MapTarToRef[i])
            {
                temp*=(1.0-(rHap.Recom[index]));
                index++;
            }
            Recom[i-1]=(1.0-temp);
        }
    }
}


void HaplotypeSet::CreateAfterUncompressSummary()
{
    NoBlocks=(int)ReducedStructureInfo.size();

    int i,j;
    maxBlockSize=0;
    maxRepSize=0;
    optEndPoints.clear();
    for(i=0;i<NoBlocks;i++)
    {
        ReducedHaplotypeInfo &TempBlock=ReducedStructureInfo[i];
        optEndPoints.push_back(TempBlock.startIndex);

        if(maxBlockSize<TempBlock.BlockSize)
            maxBlockSize=TempBlock.BlockSize;
        if(maxRepSize<TempBlock.RepSize)
            maxRepSize=TempBlock.RepSize;


        for(j=TempBlock.startIndex;j<TempBlock.endIndex;j++)
        {
            MarkerToReducedInfoMapper[j]=i;
        }

        if(i==(NoBlocks-1))
             MarkerToReducedInfoMapper[j]=i;
    }
    optEndPoints.push_back(ReducedStructureInfo[i-1].endIndex);

}



void printErr(String filename)
{
    cout<<"\n ERROR !!! \n Error in M3VCF File !!! "<<endl;
    cout<<" Please re-construct the following [.m3vcf] file using Minimac3/4 and try again ..."<<endl;
    cout<< " [ "<< filename<<" ] "<<endl;
    cout<<" Contact author if problem still persists : sayantan@umich.edu "<<endl;
    cout<<" Program Exiting ..."<<endl<<endl;
    abort();
}


void HaplotypeSet::InvertUniqueIndexMap()
{

    for(int i=0;i<NoBlocks;i++)
    {
        ReducedHaplotypeInfo &TempBlock=ReducedStructureInfo[i];
        TempBlock.uniqueIndexReverseMaps.resize(TempBlock.RepSize);
        for (int j = 0; j < numHaplotypes; j++)
        {
            TempBlock.uniqueIndexReverseMaps[TempBlock.uniqueIndexMap[j]].push_back(j);
        }

    }
}




void HaplotypeSet::CreateSiteSummary()
 {
    NoBlocks=(int)ReducedStructureInfoSummary.size();
    maxBlockSize=0;
    maxRepSize=0;


    for(int i=0;i<NoBlocks;i++)
    {
        ReducedHaplotypeInfoSummary &TempBlock=ReducedStructureInfoSummary[i];
        if(maxBlockSize<TempBlock.BlockSize)
            maxBlockSize=TempBlock.BlockSize;
        if(maxRepSize<TempBlock.RepSize)
            maxRepSize=TempBlock.RepSize;

    }

    MarkerToReducedInfoMapper.resize(numMarkers, 0);
    int i,j;
    for(i=0;i<NoBlocks;i++)
    {
        ReducedHaplotypeInfoSummary &TempBlock=ReducedStructureInfoSummary[i];
        for(j=TempBlock.startIndex;j<TempBlock.endIndex;j++)
        {
            MarkerToReducedInfoMapper[j]=i;
        }
        if(i==(NoBlocks-1))
             MarkerToReducedInfoMapper[j]=i;
    }


}




void HaplotypeSet::getm3VCFSampleNames(string line)
{

    individualName.clear();
    SampleNoHaplotypes.clear();
    CummulativeSampleNoHaplotypes.clear();
    numSamples=0;
    numHaplotypes=0;

    char * pch;
    char *end_str1;
    string tempString2,tempString,tempString3;
    int colCount=0;
    pch = strtok_r ((char*)line.c_str(),"\t", &end_str1);

    numHaplotypes=0;

    while(pch!=NULL)
    {
        colCount++;
        if(colCount>9)
        {
            numHaplotypes++;

            tempString=string(pch);
            tempString3=tempString.substr(tempString.size()-1,tempString.size()-1);
            tempString2=tempString.substr(0,tempString.size()-6);

            if(tempString3=="1")
            {
                numSamples++;
                individualName.push_back(tempString2);
                SampleNoHaplotypes.push_back(1);
            }
            else if(tempString3=="2")
            {
                SampleNoHaplotypes.back()=2;
            }
            else
            {
                cout<<endl<<"\n ERROR !!! \n Inconsistent Sample Name !!! "<<endl<<endl;
                cout<<" Haplotype Number suffix cannot be more than 2 ..."<<endl;
                cout<<" Erroneous Sample Name found : "<<tempString<<endl;
                printErr(inFileName);
            }
        }
        pch = strtok_r (NULL,"\t", &end_str1);
    }

    CummulativeSampleNoHaplotypes.resize(numSamples,0);

    for(int i=1;i<numSamples;i++)
        CummulativeSampleNoHaplotypes[i]+=CummulativeSampleNoHaplotypes[i-1]+SampleNoHaplotypes[i-1];

}




void HaplotypeSet::UpdateParameterList()
{
    MyOutFormat=&(MyAllVariables->myOutFormat);
    MyModelVariables=&(MyAllVariables->myModelVariables);
    MyHapDataVariables=&(MyAllVariables->myHapDataVariables);

}




bool HaplotypeSet::ReadBlockHeaderSummary(string &line, ReducedHaplotypeInfoSummary &tempBlocktoCheck)
{
    const char* tabSep="\t";
    vector<string> BlockPieces(numHaplotypes+9);

    MyTokenize(BlockPieces, line.c_str(), tabSep, 9);

    tempBlocktoCheck.BlockSize=GetNumVariants(BlockPieces[7]);
    tempBlocktoCheck.RepSize=GetNumReps(BlockPieces[7]);

    if(CheckBlockPosFlag(line, MyHapDataVariables->CHR, MyHapDataVariables->START, MyHapDataVariables->END)==1)
        return true;

    return false;

}


void HaplotypeSet::GetVariantInfoFromBlock(IFILE m3vcfxStream, ReducedHaplotypeInfoSummary &tempBlock, int &NoMarkersImported)
{
    string currID, rsID;
    string line;
    int blockEnterFlag=0;
    const char* tabSep="\t";
    int NewBlockSizeCount=0;

    for(int tempIndex=0;tempIndex<tempBlock.BlockSize;tempIndex++)
    {

        line.clear();
        m3vcfxStream->readLine(line);
        MyTokenize(BlockPiecesforVarInfo, line.c_str(), tabSep,9);


        if(StartedThisPanel==false || tempIndex>0)
        {
            StartedThisPanel=true;

            currID=BlockPiecesforVarInfo[0]+":"+BlockPiecesforVarInfo[1]+":"+BlockPiecesforVarInfo[3]+":"+BlockPiecesforVarInfo[4];
            rsID = BlockPiecesforVarInfo[2];
            if(rsID==".")
                rsID=currID;

            variant tempVariant;
            tempVariant.assignValues(currID,rsID,BlockPiecesforVarInfo[0],atoi(BlockPiecesforVarInfo[1].c_str()));
            tempVariant.assignRefAlt(BlockPiecesforVarInfo[3],BlockPiecesforVarInfo[4]);
            VariantList.push_back(tempVariant);


            string Info=BlockPiecesforVarInfo[7];

            double tempRecom=GetRecom(Info);
            double tempError=GetError(Info);

            if(tempRecom==-3.0)
            {
                if(MyAllVariables->myModelVariables.constantParam)
                {
                    Recom.push_back(0.01);
                    Error.push_back(0.001);
                }
                else
                {
                    cout << "\n ERROR !!! \n No parameter estimates found in M3VCF file !!!"<<endl;
                    cout << " Please use M3VCF file with parameter estimates OR use handle \"--constantParam\" to override this check ... "<<endl;
                    cout << "\n Program Exiting ... \n\n";
                    abort();
                }
            }
            else
            {
                Recom.push_back(tempRecom);
                Error.push_back(tempError);
            }

            NewBlockSizeCount++;
            NoMarkersImported++;
        }

        if(blockEnterFlag==0)
        {
            tempBlock.startIndex=NoMarkersImported-1;
            blockEnterFlag=1;
        }
        tempBlock.endIndex=NoMarkersImported-1;
    }

}



bool HaplotypeSet::ReadBlockHeader(string &line, ReducedHaplotypeInfo &tempBlocktoCheck)
{
    const char* tabSep="\t";
    vector<string> BlockPieces(numHaplotypes+9);

    MyTokenize(BlockPieces, line.c_str(), tabSep,numHaplotypes+9);
    tempBlocktoCheck.BlockSize=GetNumVariants(BlockPieces[7]);
    tempBlocktoCheck.RepSize=GetNumReps(BlockPieces[7]);

    if(CheckBlockPosFlag(line,MyHapDataVariables->CHR,MyHapDataVariables->START,MyHapDataVariables->END)==1)
        return true;


    fill(tempBlocktoCheck.uniqueCardinality.begin(), tempBlocktoCheck.uniqueCardinality.end(), 0);

    int index=0;

    while(index<numHaplotypes)
    {
        int tempval=atoi(BlockPieces[index+9].c_str());
        tempBlocktoCheck.uniqueIndexMap[index]=tempval;
        tempBlocktoCheck.uniqueCardinality[tempval]++;
        index++;
    }

    for (int i = 0; i < tempBlocktoCheck.RepSize; i++)
    {
        tempBlocktoCheck.InvuniqueCardinality[i]=1.0/(float)tempBlocktoCheck.uniqueCardinality[i];
    }

    return false;

}


void HaplotypeSet::ReadThisBlock(IFILE m3vcfxStream,
                                 int blockIndex, ReducedHaplotypeInfo &tempBlock)
{
    string line;
    const char* tabSep="\t";
    vector<string> BlockPieces(9);
    for(int tempIndex=0;tempIndex<tempBlock.BlockSize;tempIndex++)
    {
        line.clear();
        m3vcfxStream->readLine(line);
        MyTokenize(BlockPieces, line.c_str(), tabSep,9);

        vector<AlleleType> &TempHap = tempBlock.TransposedUniqueHaps[tempIndex];

        string &tempString=BlockPieces[8];

        for(int index=0;index<tempBlock.RepSize;index++)
        {
            char t=tempString[index];
            TempHap[index]=(t);
        }
    }

}




bool HaplotypeSet::BasicCheckForTargetHaplotypes(String &VCFFileName,
                                                 String TypeofFile,
                                                 AllVariable& MyAllVariable)
{
    MyAllVariables=&MyAllVariable;
    std::cout << "\n Checking "<<TypeofFile<<" haplotype file : "<<VCFFileName << endl;

    string FileType=DetectFileType(VCFFileName);

    if(FileType.compare("NA")==0)
    {
        cout << "\n ERROR !!! \n Program could NOT open file : " << VCFFileName << endl;
        cout << "\n Program Exiting ... \n\n";
        return false;
    }
    else if(FileType.compare("vcf")!=0)
    {
        cout << "\n ERROR !!! \n GWAS File provided by \"--haps\" must be a VCF file !!! \n";
        cout << " Please check the following file : "<<VCFFileName<<endl;
        cout << "\n Program Exiting ... \n\n";
        return false;
    }

    return GetVariantInfofromVCFFile(VCFFileName, TypeofFile, MyAllVariable);
}


bool HaplotypeSet::BasicCheckForVCFReferenceHaplotypes(String &VCFFileName,
                                                       String TypeofFile,
                                                       AllVariable& MyAllVariable)
{
    MyAllVariables=&MyAllVariable;
    std::cout << "\n Checking Reference haplotype file : "<<VCFFileName << endl;

    string FileType=DetectFileType(VCFFileName);

     if(FileType.compare("NA")==0)
    {
        cout << "\n ERROR !!! \n Program could NOT open file : " << VCFFileName << endl;
        cout << "\n Program Exiting ... \n\n";
        return false;
    }

    if(FileType.compare("vcf")!=0)
    {
        cout << "\n ERROR !!! \n If \"--processReference\" is ON,";
        cout << " Reference  File provided by \"--refHaps\" must be a VCF file !!! \n";
        cout << " Please check the following file : "<<VCFFileName<<endl;
        cout << "\n Program Exiting ... \n\n";
        return false;
    }

    if(MyAllVariables->myModelVariables.rounds==0)
    {

        cout <<"\n NOTE: User has specified \"--rounds\" = 0 !!!\n";
        cout<<"       No parameter estimation will be done on VCF file.\n";
        cout<<"       Program will use default estimates leading to possibly inaccurate estimates."<<endl;
    }

    return GetVariantInfofromVCFFile(VCFFileName, TypeofFile, MyAllVariable);
}


bool HaplotypeSet::BasicCheckForM3VCFReferenceHaplotypes(String &Reffilename,
                                                    AllVariable& MyAllVariable)

{
    MyAllVariables=&MyAllVariable;
    UpdateParameterList();
    std::cout << "\n Checking Reference haplotype file : "<<Reffilename << endl;
    inFileName=Reffilename;

    string refFileType=DetectFileType(Reffilename);

    if(refFileType.compare("NA")==0)
    {
        cout << "\n ERROR !!! \n Program could NOT open file : " << Reffilename << endl;
        cout << "\n Program Exiting ... \n\n";
        return false;
    }

    if(refFileType.compare("Invalid")==0)
    {
        cout << "\n ERROR !!! \n Reference File provided by \"--refHaps\" must be a M3VCF file !!! \n";
        cout << " Please check the following file : "<<Reffilename<<endl;
        cout << "\n Program Exiting ... \n\n";
        return false;
    }

    if(refFileType.compare("vcf")==0)
    {

        cout << "\n ERROR !!! \n VCF Format detected ...";
        cout << "\n The current version of Minimac4 can ONLY handle M3VCF files for imputation "<<endl;
        cout <<   " Please convert the VCF file to M3VCF using Minimac3 "<<endl;
        cout<<    " We will implement this feature in Minimac4 very soon "<<endl;
        cout << "\n Program Exiting ... \n\n";
        return false;
    }
	return true;

}



bool HaplotypeSet::GetVariantInfofromVCFFile(String &VCFFileName, String TypeofFile, AllVariable& MyAllVariable)
{
	VcfFileReader inFile;
	VcfHeader header;
	VcfRecord record;
    inFileName=VCFFileName;

	inFile.setSiteOnly(true);
    inFile.open(VCFFileName, header);


    numSamples=header.getNumSamples();
    if(numSamples==0)
    {
        cout << "\n ERROR !!! \n No samples found in "<< TypeofFile <<" File : "<<VCFFileName<<endl;
		cout << " Please check the file properly..\n";
		cout << "\n Program Exiting ... \n\n";
        return false;
    }

    for (int i = 0; i < numSamples; i++)
	{
		string tempName(header.getSampleName(i));
		individualName.push_back(tempName);
	}


   std::cout << "\n Gathering variant information ..." << endl <<endl;



    int numReadRecords = 0,numActualRecords=0;
    int bp, failFilter=0,notBiallelic=0,inconsistent=0,duplicates=0, outSideRegion = 0;
    string prevID="",currID, refAllele,altAllele,PrefrefAllele,PrevaltAllele,cno,fixCno,id;

    while (inFile.readRecord(record))
    {



        int flag=0;
        cno=record.getChromStr();
        bp=record.get1BasedPosition();
        id=record.getIDStr();
        refAllele = record.getRefStr();
        altAllele = record.getAltStr();

        // Check Valid Chromosome and single chromosome
        {
            if(numActualRecords==0)
            {
                if(!CheckValidChrom(cno))
                {
                    cout << "\n ERROR !!! \n "<< TypeofFile <<" VCF File contains chromosome : "<<cno<<endl;
                    cout << " VCF File can only contain chromosomes 1-22, X(23), Y(24) !!! "<<endl;
                    cout << "\n Program Exiting ... \n\n";
                    return false;
                }
                fixCno=cno;
                finChromosome=fixCno;
            }
            else if(fixCno!=cno)
            {
                cout << "\n ERROR !!! \n "<< TypeofFile <<" VCF File contains multiple chromosomes : "<<cno<<", "<<fixCno<<", ... "<<endl;
                cout << " Please use VCF file with single chromosome !!! "<<endl;
                cout << "\n Program Exiting ... \n\n";
                return false;
            }

        }

        // Check Window Parameters provided by user
        {
            if(MyAllVariable.myHapDataVariables.CHR!="")
            {
                if(cno.compare(MyAllVariable.myHapDataVariables.CHR.c_str())!=0)
                    flag = 1;
                else
                {
                    if(MyAllVariable.myHapDataVariables.END>0)
                    {
                        if(bp>MyAllVariable.myHapDataVariables.END || bp<MyAllVariable.myHapDataVariables.START)
                            flag = 1;
                    }
                    else
                        if(bp<MyAllVariable.myHapDataVariables.START)
                            flag = 1;
                }
            }
            if(flag==1)
                outSideRegion++;
        }


        // Check bi-allelic and FILTER
        {
             if (record.getNumAlts()>1)
            {
                notBiallelic++;
                flag = 1;
            }
            if (MyAllVariable.myHapDataVariables.passOnly && record.getFilter().getString(0).compare("PASS") != 0)
            {
                failFilter++;
                flag = 1;
            }


        }

		// Check ID for duplicate
        {
            stringstream strs3,strs4;
            strs3<<(cno);
            strs4<<(bp);
            currID=(string)strs3.str()+":"+(string)strs4.str()+":"+refAllele+":"+altAllele;
            if(id==".")
                id=currID;
            if(prevID==currID)
            {
                duplicates++;
                if(MyAllVariable.myOutFormat.verbose){cout << " WARNING !!! Duplicate Variant found chr:"<<cno<<":"<<bp<<" with identical REF = "<<refAllele <<" and ALT = "<<altAllele <<"\n";}
                if(!(MyAllVariable.myHapDataVariables.ignoreDuplicates))
                {
                    cout << "\n ERROR !!! \n Duplicate Variant found chr:"<<cno<<":"<<bp<<" with identical REF = "<<refAllele <<" and ALT = "<<altAllele <<"\n";
                    cout<<"\n Use handle \"--ignoreDuplicates\" to ignore duplicate instances ... "<<endl;
                    cout << "\n Program Exiting ... \n\n";
                    return false;
                }
                flag=1;


            }
            prevID=currID;
            PrefrefAllele=refAllele;
            PrevaltAllele=altAllele;
        }

        // Check length of SNPs REF/ALT allele
        {
            // Removed this to allow '-' and '.' in the GWAS Panel
        }

        if(flag==0)
        {
            variant thisVariant(currID,cno,bp);
            VariantList.push_back(thisVariant);
            VariantList[numReadRecords].refAlleleString=refAllele;
            VariantList[numReadRecords].altAlleleString=altAllele;
            VariantList[numReadRecords].rsid=id;
            ++numReadRecords;
        }
        numActualRecords++;
        importIndexList.push_back(flag);

    }

    importIndexListSize=(int)importIndexList.size();


    numMarkers=numReadRecords;

    if(numActualRecords==0)
    {
        cout << "\n ERROR !!! \n No variants found in "<< TypeofFile <<" File : "<<VCFFileName<<endl;
		cout << " Please check the file properly..\n";
		cout << "\n Program Exiting ... \n\n";
        return false;
    }


    inFile.close();
    SampleNoHaplotypes.resize(numSamples,2);
    CummulativeSampleNoHaplotypes.resize(numSamples,0);

    inFile.open(VCFFileName, header);
    inFile.setSiteOnly(false);
    inFile.readRecord(record);
    int tempHapCount=0;
    for (int i = 0; i<(numSamples); i++)
    {
        if(record.getNumGTs(i)==0)
        {
            std::cout << "\n ERROR !!! \n Empty Value for Individual : " << individualName[i] << " at First Marker  " << endl;
            std::cout << " Most probably a corrupted VCF file. Please check input VCF file !!! " << endl;
            cout << "\n Program Exiting ... \n\n";
            return false;
        }
        else
        {
            CummulativeSampleNoHaplotypes[i]=tempHapCount;
            SampleNoHaplotypes[i]=(record.getNumGTs(i));
            tempHapCount+=SampleNoHaplotypes[i];
        }
    }
    inFile.close();
    numHaplotypes=tempHapCount;



//    if(outSideRegion + duplicates + notBiallelic + failFilter + inconsistent>0) {cout<<endl;}
    if(MyAllVariable.myOutFormat.verbose)
    {
        if(outSideRegion>0){std::cout << " NOTE ! "<< outSideRegion<< " variants lie outside region ..." << endl;}
        if(duplicates>0){std::cout << " NOTE ! "<< duplicates<< " instance(s) of duplicated variants discarded ..." << endl;}
        if(notBiallelic>0){std::cout << " NOTE ! "<< notBiallelic<< " multi-allelic variant(s) discarded ..." << endl;}
        if(failFilter>0){std::cout << " NOTE ! "<< failFilter<< " variant(s) failed FILTER = PASS and were discarded ..." << endl;}
        if(inconsistent>0){std::cout << " NOTE ! "<< inconsistent<< " SNP(s) with inconsistent REF/ALT alleles discarded ..." << endl;}
        if(outSideRegion + duplicates + notBiallelic + failFilter + inconsistent>0) {cout<<endl;}
    }
    cout<<" Successful !!! "<<endl;


    if(numReadRecords==0)
    {
        cout << "\n ERROR !!! \n No variants left to imported from "<< TypeofFile <<" File "<<endl;
		cout << " Please check the filtering conditions properly ...\n";
		cout << "\n Program Exiting ... \n\n";
        return false;
    }

    return true;

}



bool HaplotypeSet::ScaffoldGWAStoReference(HaplotypeSet &rHap, AllVariable& MyAllVariable)
{

    int refMarkerCount=(int)rHap.VariantList.size();
    int counter = 0;
    int flag;
	int GWASOnlycounter = 0;
	int OverlapOnlycounter = 0;
	Targetmissing.resize(refMarkerCount, true);
	MapRefToTar.resize(refMarkerCount, -1);
    MapTarToRef.resize(numMarkers, -1);

    TargetMissingTypedOnly.clear();
	knownPosition.resize(numMarkers);
	OverlapOnlyVariantList.resize(numMarkers);
	TypedOnlyVariantList.resize(numMarkers);
	RefAlleleSwap.resize(numMarkers);

	for (int j = 0; j<(int)VariantList.size(); j++)
	{

		int prevCounter = counter;
		flag=0;
		while(counter<refMarkerCount && flag==0 && rHap.VariantList[counter].bp<=VariantList[j].bp)
        {

            if(rHap.VariantList[counter].chr==VariantList[j].chr
             && rHap.VariantList[counter].bp==VariantList[j].bp)
            {
                prevCounter = counter;

                if(rHap.VariantList[counter].refAlleleString==VariantList[j].refAlleleString
                        && rHap.VariantList[counter].altAlleleString==VariantList[j].altAlleleString)
                    flag=1;
                else if(rHap.VariantList[counter].refAlleleString==VariantList[j].altAlleleString
                        && rHap.VariantList[counter].altAlleleString==VariantList[j].refAlleleString)
                    flag=1;
                else if (VariantList[j].refAlleleString==VariantList[j].altAlleleString
                        && rHap.VariantList[counter].refAlleleString==VariantList[j].refAlleleString)
                    flag=1;
                else if (VariantList[j].refAlleleString==VariantList[j].altAlleleString
                        && rHap.VariantList[counter].altAlleleString==VariantList[j].refAlleleString)
                    flag=1;
                else
                    counter++;
            }
            else
                counter++;
        }


        if(flag==1)
        {
            knownPosition[j]=(counter);
            OverlapOnlyVariantList[OverlapOnlycounter]=(VariantList[j]);

            if(rHap.VariantList[counter].refAlleleString==VariantList[j].refAlleleString)
                RefAlleleSwap[OverlapOnlycounter]=(false);
            else
                {
                    RefAlleleSwap[OverlapOnlycounter]=(true);
                    string tempa=VariantList[j].refAlleleString;
                    VariantList[j].refAlleleString=VariantList[j].altAlleleString;
                    VariantList[j].altAlleleString=tempa;
                }

            Targetmissing[counter] = false;
            MapRefToTar[counter]=OverlapOnlycounter;
            MapTarToRef[OverlapOnlycounter]=counter;

            OverlapOnlycounter++;
            counter++;
		}
		else
        {
            if(MyAllVariable.myOutFormat.TypedOnly)
            {
                TypedOnlyVariantList[GWASOnlycounter]=(VariantList[j]);
                TargetMissingTypedOnly.push_back(counter-1);
                GWASOnlycounter++;
            }
            knownPosition[j]=(-1);
            counter = prevCounter;
        }

	}

    counter=0;
    rHap.RefTypedIndex.clear();
    int ThisIndex=0;

	while(counter<refMarkerCount && ThisIndex<(int)TargetMissingTypedOnly.size())
    {
        if(counter<=TargetMissingTypedOnly[ThisIndex])
        {
            rHap.RefTypedIndex.push_back(-1);
            counter++;
        }
        else
        {
            rHap.RefTypedIndex.push_back(ThisIndex);
            ThisIndex++;
        }
    }
    while(counter<refMarkerCount)
    {
        rHap.RefTypedIndex.push_back(-1);
        counter++;
    }
    while(ThisIndex<(int)TargetMissingTypedOnly.size())
    {
        rHap.RefTypedIndex.push_back(ThisIndex);
        ThisIndex++;
    }


    rHap.RefTypedTotalCount=GWASOnlycounter+rHap.numMarkers;
    if(rHap.RefTypedTotalCount!=(int)rHap.RefTypedIndex.size())
    {
        cout<<endl<<endl<<" ERROR in Code Construction [ERROR: 007] !!! "<<endl;
        cout<<" Please Contact author with ERROR number urgently : sayantan@umich.edu "<<endl;
        cout<<" Program Exiting ..."<<endl<<endl;
        abort();
    }


    numOverlapMarkers=OverlapOnlycounter;
    numTypedOnlyMarkers=GWASOnlycounter;
    RefAlleleSwap.resize(numOverlapMarkers);
    MapTarToRef.resize(numOverlapMarkers);
    OverlapOnlyVariantList.resize(numOverlapMarkers);
    TypedOnlyVariantList.resize(numTypedOnlyMarkers);


    cout<<"\n Reference Panel   : Found "<<rHap.numSamples<< " samples ("<< rHap.numHaplotypes  <<" haplotypes) and "<< (int)rHap.VariantList.size()<<" variants ..."<<endl;


    cout<<"\n Target/GWAS Panel : Found "<<numSamples<< " samples ("<< numHaplotypes  <<" haplotypes) and "<< numOverlapMarkers  + numTypedOnlyMarkers<<" variants ..."<<endl;
    cout<<"                     "<<numOverlapMarkers<<" variants overlap with Reference panel "<<endl;
    cout<<"                     "<< numTypedOnlyMarkers<<" variants imported that exist only in Target/GWAS panel"<<endl;


//    VariantList.clear();
	if (numOverlapMarkers == 0)
	{

		cout << "\n ERROR !!! \n No overlap between Target and Reference markers !!!\n";
		cout << " Please check for consistent marker identifer in reference and target input files..\n";
		cout << "\n Program Exiting ... \n\n";
        return false;

	}
	return true;

}



void HaplotypeSet::GetSummary(IFILE m3vcfxStream)
{
    vector<string> headerTag(2);
    string line;
    const char *equalSep="=";
    m3vcfxStream->readLine(line);

    bool Header=true;

    while(Header)
    {
        line.clear();
        m3vcfxStream->readLine(line);


        if(line.substr(0,2).compare("##")==0)
            Header=true;
        else
            break;

        MyTokenize(headerTag, line.c_str(), equalSep, 2);

        if(headerTag[0].compare("##n_blocks")==0)
        {
            NoBlocks=atoi(headerTag[1].c_str());
        }
    }

    getm3VCFSampleNames(line);

}






bool HaplotypeSet::ReadM3VCFChunkingInformation(String &Reffilename,string checkChr)
{
    string line;
    int blockIndex, NoMarkersImported=0;
    NoLinesToDiscardatBeginning=0;
    finChromosome="NULL";
    StartedThisPanel=false;
    AlreadyReadMiddle=false;
    BlockPiecesforVarInfo.resize(9);

   std::cout << "\n Gathering variant information ..." << endl;

    if(MyHapDataVariables->CHR!="")
    {
        std::cout << "\n Loading markers in region chr" << MyHapDataVariables->CHR<<":"<<MyHapDataVariables->START<<"-"<<MyHapDataVariables->END<<" ..."<< endl;
    }

    IFILE m3vcfxStream = ifopen(Reffilename, "r");

    if(m3vcfxStream)
    {

        GetSummary(m3vcfxStream);
        if(numSamples==0)
        {
            cout << "\n ERROR !!! \n No samples found in M3VCF Input File  : "<<Reffilename<<endl;
            cout << " Please check the file properly..\n";
            cout << "\n Program Exiting ... \n\n";
            return false;
        }

        ReducedStructureInfoSummary.clear();
        for(blockIndex=0;blockIndex<NoBlocks;blockIndex++)
        {
            line.clear();
            m3vcfxStream->readLine(line);


            ReducedHaplotypeInfoSummary tempBlock;
            if(ReadBlockHeaderSummary(line, tempBlock))
            {

                if(!AlreadyReadMiddle)
                    NoLinesToDiscardatBeginning += (tempBlock.BlockSize+1);
                for(int tempIndex=0;tempIndex<tempBlock.BlockSize;tempIndex++)
                    m3vcfxStream->discardLine();
                continue;
            }

            AlreadyReadMiddle=true;
            if(blockIndex==0)
            {
               if(finChromosome!=checkChr)
               {
                    cout << "\n ERROR !!! \n Reference Panel is on chromosome "<<finChromosome<<" which is ";
                    cout <<" different from chromosome "<< checkChr<<" of the GWAS panel  "<<endl;
                    cout << " Please check the file properly..\n";
                    cout << "\n Program Exiting ... \n\n";
                    return false;

               }
            }

            GetVariantInfoFromBlock(m3vcfxStream, tempBlock, NoMarkersImported);
            ReducedStructureInfoSummary.push_back(tempBlock);

        }



    }
    numMarkers=VariantList.size();

    if (numMarkers == 0)
	{
        cout << "\n ERROR !!! \n No variants left to imported from reference haplotype file "<<endl;
		cout << " Please check the filtering conditions OR the file properly ...\n";
		cout << "\n Program Exiting ... \n\n";
        return false;
    }

    ifclose(m3vcfxStream);

    CreateSiteSummary();

    cout<<"\n Successful !!! "<<endl;

    return true;
}



bool HaplotypeSet::CheckValidChrom(string chr)
{
    bool result=false;

    if(MyAllVariables->myHapDataVariables.MyChromosome!="" && chr==MyAllVariables->myHapDataVariables.MyChromosome.c_str())
        return true;

    string temp[]={"1","2","3","4","5","6","7","8","9","10","11"
                    ,"12","13","14","15","16","17","18","19","20","21","22","23","X","Y"};
    std::vector<string> ValidChromList (temp, temp + sizeof(temp) / sizeof(string) );

    for(int counter=0;counter<(int)ValidChromList.size();counter++)
        if(chr==ValidChromList[counter])
            result=true;

    return result;

}

void HaplotypeSet::writem3vcfFile(String filename,bool &gzip)
{

    IFILE m3vcffile = ifopen(filename + ".m3vcf" + (gzip ? ".gz" : ""), "wb",(gzip ? InputFile::BGZF : InputFile::UNCOMPRESSED));
    ifprintf(m3vcffile, "##fileformat=M3VCF\n");
    ifprintf(m3vcffile, "##version=1.2\n");
    ifprintf(m3vcffile, "##compression=block\n");
    ifprintf(m3vcffile, "##n_blocks=%d\n",NoBlocks);
    ifprintf(m3vcffile, "##n_haps=%d\n",numHaplotypes);
    ifprintf(m3vcffile, "##n_markers=%d\n",numMarkers);
    if(finChromosome=="X" || finChromosome=="23")
        ifprintf(m3vcffile, "##chrxRegion=%s\n",PseudoAutosomal?"PseudoAutosomal":"NonPseudoAutosomal");
    ifprintf(m3vcffile, "##<Note=This is NOT a VCF File and cannot be read by vcftools>\n");
    ifprintf(m3vcffile, "#CHROM\t");
    ifprintf(m3vcffile, "POS\t");
    ifprintf(m3vcffile, "ID\t");
    ifprintf(m3vcffile, "REF\t");
    ifprintf(m3vcffile, "ALT\t");
    ifprintf(m3vcffile, "QUAL\t");
    ifprintf(m3vcffile, "FILTER\t");
    ifprintf(m3vcffile, "INFO\t");
    ifprintf(m3vcffile, "FORMAT");
    int i,j,k;

    for(i=0;i<(int)numSamples;i++)
    {
        ifprintf(m3vcffile, "\t%s_HAP_1",individualName[i].c_str());
        if(SampleNoHaplotypes[i]==2)
            ifprintf(m3vcffile, "\t%s_HAP_2",individualName[i].c_str());
    }
    ifprintf(m3vcffile, "\n");

    int length=NoBlocks;
    string cno;

    for(i=0;i<length;i++)
    {

        ReducedHaplotypeInfo &tempInfo = ReducedStructureInfo[i];

        cno=VariantList[tempInfo.startIndex].chr;
        int nvariants=tempInfo.BlockSize;
        int reps=tempInfo.RepSize;


        ifprintf(m3vcffile, "%s\t",cno.c_str());
        ifprintf(m3vcffile, "%d-%d\t",VariantList[tempInfo.startIndex].bp,VariantList[tempInfo.endIndex].bp);
        ifprintf(m3vcffile, "<BLOCK:%d-%d>\t.\t.\t.\t.\t",tempInfo.startIndex,tempInfo.endIndex);

        ifprintf(m3vcffile, "B%d;VARIANTS=%d;REPS=%d\t.",i+1,nvariants,reps);


        for(j=0;j<numHaplotypes;j++)
            ifprintf(m3vcffile, "\t%d",tempInfo.uniqueIndexMap[j]);

        ifprintf(m3vcffile, "\n");

        for(j=0;j<nvariants;j++)
        {
            ifprintf(m3vcffile, "%s\t",cno.c_str());
            ifprintf(m3vcffile, "%d\t",VariantList[j+tempInfo.startIndex].bp);
            ifprintf(m3vcffile, "%s\t",MyAllVariables->myOutFormat.RsId?VariantList[j+tempInfo.startIndex].rsid.c_str():VariantList[j+tempInfo.startIndex].name.c_str());
            ifprintf(m3vcffile, "%s\t%s\t.\t.\t",VariantList[j+tempInfo.startIndex].refAlleleString.c_str(),VariantList[j+tempInfo.startIndex].altAlleleString.c_str());

            ifprintf(m3vcffile, "B%d.M%d",i+1,j+1);
            if(Error.size()>0)
                ifprintf(m3vcffile, ";Err=%.5g;Recom=%.5g",
                     Error[j+tempInfo.startIndex],(j+tempInfo.startIndex)<(int)Recom.size()?Recom[j+tempInfo.startIndex]:0);
            ifprintf(m3vcffile, "\t");

            vector<AlleleType> &TempHap = tempInfo.TransposedUniqueHaps[j];
            for(k=0;k<reps;k++)
            {
                ifprintf(m3vcffile,"%c",TempHap[k]);
            }
            ifprintf(m3vcffile, "\n");
        }
    }

    std::cout << " Successfully written file ... "<<endl;
    ifclose(m3vcffile);

}

string HaplotypeSet::DetectFileType(String filename)
{
    IFILE fileStream = ifopen(filename, "r");
//    IFILE fileStream = NULL;
    string line;

//    cout<<" WELL = "<<endl;
//    abort();
    if(fileStream)
    {

        fileStream->readLine(line);
        if(line.length()<1)
            {
                ifclose(fileStream);
                return "Invalid";
            }
        string tempString;
        tempString=(line.substr(0,17));

        char temp[tempString.length() + 1];
        std::strcpy(temp,tempString.c_str());
        for (char *iter = temp; *iter != '\0'; ++iter)
        {
           *iter = std::tolower(*iter);
        }
        if(((string)temp).compare("##fileformat=m3vc")==0)
        {
            ifclose(fileStream);
            return "m3vcf";
        }
        else if(((string)temp).compare("##fileformat=vcfv")==0)
        {
            ifclose(fileStream);
            return "vcf";
        }
        else
        {
            ifclose(fileStream);
            return "Invalid";
        }

    }
    else
    {
        ifclose(fileStream);
        return "NA";
    }

    ifclose(fileStream);
    return "NA";
}



void HaplotypeSet::CalculateAlleleFreq()
{
    fill(AlleleFreq.begin(), AlleleFreq.end(), 0.0);

    int i,j,k;
    for(k=0;k<NoBlocks;k++)
    {

        ReducedHaplotypeInfo &TempBlock=ReducedStructureInfo[k];
        for(j=TempBlock.startIndex;j<TempBlock.endIndex + (k==(NoBlocks-1)? 1:0) ;j++)
        {
            vector<AlleleType> &TempHap = TempBlock.TransposedUniqueHaps[j-TempBlock.startIndex];
            for (i = 0; i<TempBlock.RepSize; i++)
            {
                if(TempHap[i]=='1')
                {
                    AlleleFreq[j]+=TempBlock.uniqueCardinality[i];
                }
            }
        }
    }

	for (int i = 0; i<numMarkers; i++)
	{
		AlleleFreq[i] /= (double)numHaplotypes;
	}

}



void HaplotypeSet::CalculateGWASOnlyAlleleFreq()
{

    fill(GWASOnlyAlleleFreq.begin(), GWASOnlyAlleleFreq.end(), 0.0);
    vector<int> TotalSample(numTypedOnlyMarkers, 0);

    int i,K,j;
    int haplotypeIndex=0;
    for(K=0;K<numSamples;K++)
    {
        for(j=0; j<(*SampleNoHaplotypesPointer)[K];j++)
        {
            for (i = 0; i<numTypedOnlyMarkers; i++)
            {
                if(GWASOnlyMissingSampleUnscaffolded[haplotypeIndex][i]=='0')
                {
                    TotalSample[i]++;
                    if(GWASOnlyhaplotypesUnscaffolded[haplotypeIndex][i]=='1')
                        GWASOnlyAlleleFreq[i]++;
                }
            }
            haplotypeIndex++;
        }
    }

    for (int i = 0; i<numTypedOnlyMarkers; i++)
	{
		GWASOnlyAlleleFreq[i]/=(double)TotalSample[i];
	}
}


AlleleType HaplotypeSet::RetrieveMissingScaffoldedHaplotype(int sample,int marker)
{
    return MissingSampleUnscaffolded[sample][marker];
}

AlleleType HaplotypeSet::RetrieveScaffoldedHaplotype(int sample,int marker)
{
    return haplotypesUnscaffolded[sample][marker];
}



void HaplotypeSet::MyTokenize(vector<string> &result, const char *input, const char *delimiter, int Number)
{

    size_t wordCount = 1;
    result[0].clear();
    std::string *word = &result[0];


    while (*input)
    {
        if (*input==*delimiter)
        {
            // we got a delimeter, and since an empty word following
            // a delimeter still counts as a word, we allocate it here
            wordCount++;

            if((int)wordCount>Number)
                return;

            result[wordCount-1].clear();
            word = &result[wordCount-1];
        }
        else
        {
            word->push_back(*input);
        }
        input++;
    }

}

string HaplotypeSet::FindTokenWithPrefix(const char *input,const char *delimiter, string CheckPrefix)
{

    std::string word = "";
    int Size = (int)CheckPrefix.size();
    while (*input)
    {
        if (*input==*delimiter)
        {
            int Index=0;
            ++input;
            while(*input)
            {
                word=word + (*input);
                if(Index<Size && *input!=CheckPrefix[Index++])
                    break;

                ++input;
                if(*input==*delimiter || *input=='\0')
                    return word;
            }
        }
        input++;
        word="";
    }
    return word;

}

int HaplotypeSet::CheckBlockPosFlag(string &input, String &CHR, int &START, int &END)
{

    const char *dashSep="-", *tabSep="\t";
    vector<string> tokens(2);
    vector<string> PosTokens(2);

    MyTokenize(tokens, input.c_str(), tabSep,2);
    string tempChr=tokens[0];

    MyTokenize(PosTokens, tokens[1].c_str(), dashSep, 2);
    int tempStartBlock=atoi(PosTokens[0].c_str());
    int tempEndBlock=atoi(PosTokens[1].c_str());

    if(CHR!="")
    {
        if(tempChr.compare(CHR.c_str())!=0)
            return 1;
        else
        {
            if(tempStartBlock>END)
                    return 1;
            if(tempEndBlock<START)
                return 1;
        }
    }

    if(finChromosome=="NULL")
        finChromosome=tempChr;

    return 0;

}


int HaplotypeSet::GetNumVariants(string &input)
{
    const char *equalSep="=",*semicolSep=";";


    string PrefixString = FindTokenWithPrefix(input.c_str(),semicolSep, "VARIANTS=");
    if(PrefixString!="")
    {
        vector<string> NoVariantsToken(2);
        MyTokenize(NoVariantsToken, PrefixString.c_str(), equalSep, 2);
        return atof(NoVariantsToken[1].c_str());
    }
    else
    {
        abort();
        cout<<endl<<endl<<" ERROR !!! \n ERROR in M3VCF File [ERROR Code : 3278] !!! "<<endl;
        cout<<" Please Contact author with ERROR number : sayantan@umich.edu "<<endl;
        cout<<" Program Exiting ..."<<endl<<endl;
        abort();
    }
}

int HaplotypeSet::GetNumReps(string &input)
{
    const char *equalSep="=",*semicolSep=";";

    string PrefixString = FindTokenWithPrefix(input.c_str(),semicolSep, "REPS=");
    if(PrefixString!="")
    {
        vector<string> NoVariantsToken(2);
        MyTokenize(NoVariantsToken, PrefixString.c_str(), equalSep, 2);
        return atof(NoVariantsToken[1].c_str());
    }
    else
    {
        cout<<endl<<endl<<" ERROR !!! \n ERROR in M3VCF File [ERROR Code : 1472] !!! "<<endl;
        cout<<" Please Contact author with ERROR number : sayantan@umich.edu "<<endl;
        cout<<" Program Exiting ..."<<endl<<endl;
        abort();
    }
}

double HaplotypeSet::GetRecom(string &input)
{

    const char *equalSep="=",*semicolSep=";";


    string PrefixString = FindTokenWithPrefix(input.c_str(),semicolSep, "Recom=");
    if(PrefixString!="")
    {
        vector<string> NoVariantsToken(2);
        MyTokenize(NoVariantsToken, PrefixString.c_str(), equalSep, 2);
        return atof(NoVariantsToken[1].c_str());
    }
    else
        return -3.0;

}


double HaplotypeSet::GetError(string &input)
{
    const char *equalSep="=",*semicolSep=";";


    string PrefixString = FindTokenWithPrefix(input.c_str(),semicolSep, "Err=");
    if(PrefixString!="")
    {
        vector<string> NoVariantsToken(2);
        MyTokenize(NoVariantsToken, PrefixString.c_str(), equalSep, 2);
        return atof(NoVariantsToken[1].c_str());
    }
    else
        return -3.0;

}



