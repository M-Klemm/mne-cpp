//=============================================================================================================
/**
* @file     fiff_stream.cpp
* @author   Christoph Dinh <chdinh@nmr.mgh.harvard.edu>;
*           Matti Hamalainen <msh@nmr.mgh.harvard.edu>
* @version  1.0
* @date     July, 2012
*
* @section  LICENSE
*
* Copyright (C) 2012, Christoph Dinh and Matti Hamalainen. All rights reserved.
*
* Redistribution and use in source and binary forms, with or without modification, are permitted provided that
* the following conditions are met:
*     * Redistributions of source code must retain the above copyright notice, this list of conditions and the
*       following disclaimer.
*     * Redistributions in binary form must reproduce the above copyright notice, this list of conditions and
*       the following disclaimer in the documentation and/or other materials provided with the distribution.
*     * Neither the name of the Massachusetts General Hospital nor the names of its contributors may be used
*       to endorse or promote products derived from this software without specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED
* WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
* PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL MASSACHUSETTS GENERAL HOSPITAL BE LIABLE FOR ANY DIRECT,
* INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
* PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
* HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
* NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
* POSSIBILITY OF SUCH DAMAGE.
*
*
* @brief    Implementation of the FiffStream Class.
*
*/


//*************************************************************************************************************
//=============================================================================================================
// MNE INCLUDES
//=============================================================================================================

#include "fiff_stream.h"
#include "fiff_tag.h"
#include "fiff_dir_tree.h"
#include "fiff_ctf_comp.h"
#include "fiff_info.h"
#include "fiff_info_base.h"
#include "fiff_raw_data.h"

#include <mneMath/mnemath.h>


//*************************************************************************************************************
//=============================================================================================================
// Qt INCLUDES
//=============================================================================================================

#include <QFile>


//*************************************************************************************************************
//=============================================================================================================
// USED NAMESPACES
//=============================================================================================================

using namespace FIFFLIB;
using namespace MNEMATHLIB;


//*************************************************************************************************************
//=============================================================================================================
// DEFINE MEMBER METHODS
//=============================================================================================================

FiffStream::FiffStream(QIODevice* p_pIODevice)
: QDataStream(p_pIODevice)
{
    this->setFloatingPointPrecision(QDataStream::SinglePrecision);
    this->setByteOrder(QDataStream::BigEndian);
    this->setVersion(QDataStream::Qt_5_0);
}


//*************************************************************************************************************

FiffStream::FiffStream(QByteArray * a, QIODevice::OpenMode mode)
: QDataStream(a, mode)
{
    this->setFloatingPointPrecision(QDataStream::SinglePrecision);
    this->setByteOrder(QDataStream::BigEndian);
    this->setVersion(QDataStream::Qt_5_0);
}


//*************************************************************************************************************

FiffStream::~FiffStream()
{
    //ToDo check if all IO devices are closed outside --> don't do this here!!
//    printf("DEBUG: check if FiffStream::IODevice is closed else where. Cause here it's not anymore.");

//    if(this->device()->isOpen())
//    {
//        printf("DEBUG: Closing FiffStream %s.\n\n", this->streamName().toUtf8().constData());
//        this->device()->close();
//    }
}


//*************************************************************************************************************

void FiffStream::end_block(fiff_int_t kind)
{
    this->write_int(FIFF_BLOCK_END,&kind);
}


//*************************************************************************************************************

void FiffStream::end_file()
{
    fiff_int_t datasize = 0;

    *this << (qint32)FIFF_NOP;
    *this << (qint32)FIFFT_VOID;
    *this << (qint32)datasize;
    *this << (qint32)FIFFV_NEXT_NONE;
}


//*************************************************************************************************************

void FiffStream::finish_writing_raw()
{
    this->end_block(FIFFB_RAW_DATA);
    this->end_block(FIFFB_MEAS);
    this->end_file();
}


//*************************************************************************************************************

bool FiffStream::open(FiffDirTree& p_Tree, QList<FiffDirEntry>& p_Dir)
{
    QString t_sFileName = this->streamName();

    if (!this->device()->open(QIODevice::ReadOnly))
    {
        printf("Cannot open %s\n", t_sFileName.toUtf8().constData());//consider throw
        return false;
    }

    FIFFLIB::FiffTag* t_pTag = NULL;
    FiffTag::read_tag_info(this, t_pTag);

    if (t_pTag->kind != FIFF_FILE_ID)
    {
        printf("Fiff::open: file does not start with a file id tag");//consider throw
        return false;
    }

    if (t_pTag->type != FIFFT_ID_STRUCT)
    {
        printf("Fiff::open: file does not start with a file id tag");//consider throw
        return false;
    }
    if (t_pTag->size() != 20)
    {
        printf("Fiff::open: file does not start with a file id tag");//consider throw
        return false;
    }

    FiffTag::read_tag(this, t_pTag);

    if (t_pTag->kind != FIFF_DIR_POINTER)
    {
        printf("Fiff::open: file does have a directory pointer");//consider throw
        return false;
    }

    //
    //   Read or create the directory tree
    //
    printf("\nCreating tag directory for %s...", t_sFileName.toUtf8().constData());

    p_Dir.clear();
    qint32 dirpos = *t_pTag->toInt();
    if (dirpos > 0)
    {
        FiffTag::read_tag(this, t_pTag, dirpos);
        p_Dir = t_pTag->toDirEntry();
    }
    else
    {
        qint32 k = 0;
        this->device()->seek(0);//fseek(fid,0,'bof');
        FiffDirEntry t_fiffDirEntry;
        while (t_pTag->next >= 0)
        {
            t_fiffDirEntry.pos = this->device()->pos();//pos = ftell(fid);
            FiffTag::read_tag_info(this, t_pTag);
            ++k;
            t_fiffDirEntry.kind = t_pTag->kind;
            t_fiffDirEntry.type = t_pTag->type;
            t_fiffDirEntry.size = t_pTag->size();
            p_Dir.append(t_fiffDirEntry);
        }
    }
    delete t_pTag;
    //
    //   Create the directory tree structure
    //

    FiffDirTree::make_dir_tree(this, p_Dir, p_Tree);

    printf("[done]\n");

    //
    //   Back to the beginning
    //
    this->device()->seek(0); //fseek(fid,0,'bof');
    return true;
}


//*************************************************************************************************************

QStringList FiffStream::read_bad_channels(const FiffDirTree& p_Node)
{
    QList<FiffDirTree> node = p_Node.dir_tree_find(FIFFB_MNE_BAD_CHANNELS);
    FIFFLIB::FiffTag* t_pTag = NULL;

    QStringList bads;

    if (node.size() > 0)
        if(node[0].find_tag(this, FIFF_MNE_CH_NAME_LIST, t_pTag))
            bads = split_name_list(t_pTag->toString());

    if(t_pTag)
        delete t_pTag;

    return bads;
}


//*************************************************************************************************************

bool FiffStream::read_cov(const FiffDirTree& p_Node, fiff_int_t cov_kind, FiffCov& p_covData)
{
    p_covData.clear();

    //
    //   Find all covariance matrices
    //
    QList<FiffDirTree> covs = p_Node.dir_tree_find(FIFFB_MNE_COV);
    if (covs.size() == 0)
    {
        printf("No covariance matrices found");
        return false;
    }
    //
    //   Is any of the covariance matrices a noise covariance
    //
    qint32 p = 0;
    FiffTag* tag = NULL;
    bool success = false;
    fiff_int_t dim, nfree, nn;
    QStringList names;
    bool diagmat = false;
    VectorXd eig;
    MatrixXd eigvec;
    VectorXd cov_diag;
    MatrixXd cov;
    VectorXd cov_sparse;
    QStringList bads;
    for(p = 0; p < covs.size(); ++p)
    {
        success = covs[p].find_tag(this, FIFF_MNE_COV_KIND, tag);
        if (success && *tag->toInt() == cov_kind)
        {
            FiffDirTree* current = &covs[p];
            //
            //   Find all the necessary data
            //
            if (!current->find_tag(this, FIFF_MNE_COV_DIM, tag))
            {
                printf("Covariance matrix dimension not found.\n");
                return false;
            }
            dim = *tag->toInt();
            if (!current->find_tag(this, FIFF_MNE_COV_NFREE, tag))
                nfree = -1;
            else
                nfree = *tag->toInt();

            if (current->find_tag(this, FIFF_MNE_ROW_NAMES, tag))
            {
                names = FiffStream::split_name_list(tag->toString());
                if (names.size() != dim)
                {
                    printf("Number of names does not match covariance matrix dimension\n");
                    return false;
                }
            }
            if (!current->find_tag(this, FIFF_MNE_COV, tag))
            {
                if (!current->find_tag(this, FIFF_MNE_COV_DIAG, tag))
                {
                    printf("No covariance matrix data found\n");
                    return false;
                }
                else
                {
                    //
                    //   Diagonal is stored
                    //
                    if (tag->type == FIFFT_DOUBLE)
                    {
                        cov_diag = Map<VectorXd>(tag->toDouble(),dim);
                    }
                    else if (tag->type == FIFFT_FLOAT)
                    {
                        cov_diag = Map<VectorXf>(tag->toFloat(),dim).cast<double>();
                    }
                    else {
                        printf("Illegal data type for covariance matrix\n");
                        return false;
                    }

                    diagmat = true;
                    printf("\t%d x %d diagonal covariance (kind = %d) found.\n", dim, dim, cov_kind);
                }
            }
            else
            {
                VectorXd vals;
                nn = dim*(dim+1)/2;
                if (tag->type == FIFFT_DOUBLE)
                {
                    vals =  Map<VectorXd>(tag->toDouble(),nn);
                }
                else if (tag->type == FIFFT_FLOAT)
                {
                    vals = Map<VectorXf>(tag->toFloat(),nn).cast<double>();
                }
                else
                {
                    qDebug() << tag->getInfo();
                    return false;
                }

                if(!MNEMath::issparse(vals))
                {
                    //
                    //   Lower diagonal is stored
                    //
                    cov = MatrixXd::Zero(dim,dim);

                    // XXX : should remove for loops
                    qint32 q = 0;
                    for(qint32 j = 0; j < dim; ++j)
                    {
                        for(qint32 k = 0; k <= j; ++k)
                        {
                            cov(j,k) = vals(q);
                            ++q;
                        }
                    }
                    for(qint32 j = 0; j < dim; ++j)
                        for(qint32 k = j+1; k < dim; ++k)
                            cov(j,k) = cov(k,j);

                    diagmat = false;
                    printf("\t%d x %d full covariance (kind = %d) found.\n", dim, dim, cov_kind);

                }
                else
                {
                    diagmat = false;
                    qDebug() << "ToDo: FiffStream::read_cov - this needs to be debugged.\n";
                    cov = vals;
                    printf("\t%d x %d sparse covariance (kind = %d) found.\n", dim, dim, cov_kind);
                }
//MATLAB
//                    if ~issparse(tag.data)
//                        //
//                        //   Lower diagonal is stored
//                        //
//                        qDebug() << tag->getInfo();
//                        vals = tag.data;
//                        data = zeros(dim,dim);
//                        % XXX : should remove for loops
//                        q = 1;
//                        for j = 1:dim
//                            for k = 1:j
//                                data(j,k) = vals(q);
//                                q = q + 1;
//                            end
//                        end
//                        for j = 1:dim
//                            for k = j+1:dim
//                                data(j,k) = data(k,j);
//                            end
//                        end
//                        diagmat = false;
//                        fprintf('\t%d x %d full covariance (kind = %d) found.\n',dim,dim,cov_kind);
//                    else
//                        diagmat = false;
//                        data = tag.data;
//                        fprintf('\t%d x %d sparse covariance (kind = %d) found.\n',dim,dim,cov_kind);
//                    end
//MATLAB END
            }
            //
            //   Read the possibly precomputed decomposition
            //
            FiffTag* tag1 = NULL;
            FiffTag* tag2 = NULL;
            if (current->find_tag(this, FIFF_MNE_COV_EIGENVALUES, tag1) && current->find_tag(this, FIFF_MNE_COV_EIGENVECTORS, tag2))
            {
                eig = VectorXd(Map<VectorXd>(tag1->toDouble(),dim));
                eigvec = tag2->toFloatMatrix();
                eigvec.transposeInPlace();
            }
            //
            //   Read the projection operator
            //
            QList<FiffProj> projs = this->read_proj(*current);
            //
            //   Read the bad channel list
            //
            bads = this->read_bad_channels(*current);
            //
            //   Put it together
            //
            p_covData.clear();

            p_covData.kind   = cov_kind;
            p_covData.diag   = diagmat;
            p_covData.dim    = dim;
            p_covData.names  = names;

            if(cov_diag.size() > 0)
                p_covData.data   = cov_diag;
            else if(cov.size() > 0)
                p_covData.data   = cov;
            else if(cov_sparse.size() > 0)
                p_covData.data   = cov_sparse;

            p_covData.projs  = projs;
            p_covData.bads   = bads;
            p_covData.nfree  = nfree;
            p_covData.eig    = eig;
            p_covData.eigvec = eigvec;

            if (tag)
                delete tag;
            //
            return true;
        }
    }

    printf("Did not find the desired covariance matrix\n");
    return false;
}


//*************************************************************************************************************

QList<FiffCtfComp> FiffStream::read_ctf_comp(const FiffDirTree& p_Node, const QList<FiffChInfo>& p_Chs)
{
    QList<FiffCtfComp> compdata;
    QList<FiffDirTree> t_qListComps = p_Node.dir_tree_find(FIFFB_MNE_CTF_COMP_DATA);

    qint32 i, k, p, col, row;
    fiff_int_t kind, pos;
    FiffTag* t_pTag = NULL;
    for (k = 0; k < t_qListComps.size(); ++k)
    {
        FiffDirTree* node = &t_qListComps[k];
        //
        //   Read the data we need
        //
        FiffNamedMatrix::SDPtr mat(new FiffNamedMatrix());
        this->read_named_matrix(*node, FIFF_MNE_CTF_COMP_DATA, *mat.data());
        for(p = 0; p < node->nent; ++p)
        {
            kind = node->dir.at(p).kind;
            pos  = node->dir.at(p).pos;
            if (kind == FIFF_MNE_CTF_COMP_KIND)
            {
                FiffTag::read_tag(this, t_pTag, pos);
                break;
            }
        }
        if (!t_pTag)
        {
            printf("Compensation type not found\n");
            return compdata;
        }
        //
        //   Get the compensation kind and map it to a simple number
        //
        FiffCtfComp one;
        one.ctfkind = *t_pTag->toInt();
        delete t_pTag;
        t_pTag = NULL;

        one.kind   = -1;
        if (one.ctfkind == 1194410578) //hex2dec('47314252')
            one.kind = 1;
        else if (one.ctfkind == 1194476114) //hex2dec('47324252')
            one.kind = 2;
        else if (one.ctfkind == 1194541650) //hex2dec('47334252')
            one.kind = 3;
        else
            one.kind = one.ctfkind;

        for (p = 0; p < node->nent; ++p)
        {
            kind = node->dir.at(p).kind;
            pos  = node->dir.at(p).pos;
            if (kind == FIFF_MNE_CTF_COMP_CALIBRATED)
            {
                FiffTag::read_tag(this, t_pTag, pos);
                break;
            }
        }
        bool calibrated;
        if (!t_pTag)
            calibrated = false;
        else
            calibrated = (bool)*t_pTag->toInt();

        one.save_calibrated = calibrated;
        one.rowcals = MatrixXd::Ones(1,mat->data.rows());//ones(1,size(mat.data,1));
        one.colcals = MatrixXd::Ones(1,mat->data.cols());//ones(1,size(mat.data,2));
        if (!calibrated)
        {
            //
            //   Calibrate...
            //
            //
            //   Do the columns first
            //
            QStringList ch_names;
            for (p  = 0; p < p_Chs.size(); ++p)
                ch_names.append(p_Chs[p].ch_name);

            qint32 count;
            MatrixXd col_cals(mat->data.cols(), 1);
            col_cals.setZero();
            for (col = 0; col < mat->data.cols(); ++col)
            {
                count = 0;
                for (i = 0; i < ch_names.size(); ++i)
                {
                    if (QString::compare(mat->col_names.at(col),ch_names.at(i)) == 0)
                    {
                        count += 1;
                        p = i;
                    }
                }
                if (count == 0)
                {
                    printf("Channel %s is not available in data",mat->col_names.at(col).toUtf8().constData());
                    delete t_pTag;
                    return compdata;
                }
                else if (count > 1)
                {
                    printf("Ambiguous channel %s",mat->col_names.at(col).toUtf8().constData());
                    delete t_pTag;
                    return compdata;
                }
                col_cals(col,0) = 1.0f/(p_Chs[p].range*p_Chs[p].cal);
            }
            //
            //    Then the rows
            //
            MatrixXd row_cals(mat->data.rows(), 1);
            row_cals.setZero();
            for (row = 0; row < mat->data.rows(); ++row)
            {
                count = 0;
                for (i = 0; i < ch_names.size(); ++i)
                {
                    if (QString::compare(mat->row_names.at(row),ch_names.at(i)) == 0)
                    {
                        count += 1;
                        p = i;
                    }
                }

                if (count == 0)
                {
                    printf("Channel %s is not available in data",mat->row_names.at(row).toUtf8().constData());
                    delete t_pTag;
                    return compdata;
                }
                else if (count > 1)
                {
                    printf("Ambiguous channel %s",mat->row_names.at(row).toUtf8().constData());
                    delete t_pTag;
                    return compdata;
                }

                row_cals(row, 0) = p_Chs[p].range*p_Chs[p].cal;
            }
            mat->data           = row_cals.asDiagonal()* mat->data *col_cals.asDiagonal();
            one.rowcals         = row_cals;
            one.colcals         = col_cals;
        }
        one.data       = mat;
        compdata.append(one);
    }

    if (compdata.size() > 0)
        printf("\tRead %d compensation matrices\n",compdata.size());

    if(t_pTag)
        delete t_pTag;
    return compdata;
}


//*************************************************************************************************************

bool FiffStream::read_meas_info_base(const FiffDirTree& p_Node, FiffInfoBase& p_InfoForward)
{
    p_InfoForward.clear();

    //
    //   Find the desired blocks
    //
    QList<FiffDirTree> parent_meg = p_Node.dir_tree_find(FIFFB_MNE_PARENT_MEAS_FILE);

    if (parent_meg.size() == 0)
    {
        printf("No parent MEG information found in operator\n");
        return false;
    }

    FiffTag* t_pTag = NULL;

    QList<FiffChInfo> chs;
    FiffCoordTrans cand;
    fiff_int_t kind = -1;
    fiff_int_t pos = -1;

    for (qint32 k = 0; k < parent_meg[0].nent; ++k)
    {
        kind = parent_meg[0].dir[k].kind;
        pos  = parent_meg[0].dir[k].pos;
        if (kind == FIFF_CH_INFO)
        {
            FiffTag::read_tag(this, t_pTag, pos);
            chs.append( t_pTag->toChInfo() );
        }
    }

    //
    //   Add the channel information and make a list of channel names
    //   for convenience
    //
    p_InfoForward.chs = chs;
    for (qint32 c = 0; c < p_InfoForward.chs.size(); ++c)
        p_InfoForward.ch_names << p_InfoForward.chs[c].ch_name;

    p_InfoForward.nchan = chs.size();

    //
    //   Get the MEG device <-> head coordinate transformation
    //
    if(parent_meg[0].find_tag(this, FIFF_COORD_TRANS, t_pTag))
    {
        cand = t_pTag->toCoordTrans();
        if(cand.from == FIFFV_COORD_DEVICE && cand.to == FIFFV_COORD_HEAD)
            p_InfoForward.dev_head_t = cand;
        else if (cand.from == FIFFV_MNE_COORD_CTF_HEAD && cand.to == FIFFV_COORD_HEAD)
            p_InfoForward.ctf_head_t = cand;
        else
            printf("MEG device/head coordinate transformation not found");
    }
    else
        printf("MEG/head coordinate transformation not found.\n");

    //
    //   Load the bad channel list
    //
    p_InfoForward.bads = this->read_bad_channels(p_Node);

    //Garbage Collection
    if (t_pTag)
        delete t_pTag;

    return true;
}


//*************************************************************************************************************

bool FiffStream::read_meas_info(const FiffDirTree& p_Node, FiffInfo& info, FiffDirTree& p_NodeInfo)
{
//    if (info)
//        delete info;
    info.clear();
    //
    //   Find the desired blocks
    //
    QList<FiffDirTree> meas = p_Node.dir_tree_find(FIFFB_MEAS);

    if (meas.size() == 0)
    {
        printf("Could not find measurement data\n");
        return false;
    }
    //
    QList<FiffDirTree> meas_info = meas[0].dir_tree_find(FIFFB_MEAS_INFO);
    if (meas_info.count() == 0)
    {
        printf("Could not find measurement info\n");
//        delete meas[0];
        return false;
    }
    //
    //   Read measurement info
    //
    FiffTag* t_pTag = NULL;

    fiff_int_t nchan = -1;
    float sfreq = -1.0f;
    QList<FiffChInfo> chs;
    float lowpass = -1.0f;
    float highpass = -1.0f;

    FiffChInfo t_chInfo;

    FiffCoordTrans cand;// = NULL;
    FiffCoordTrans dev_head_t;// = NULL;
    FiffCoordTrans ctf_head_t;// = NULL;

    fiff_int_t meas_date[2];
    meas_date[0] = -1;
    meas_date[1] = -1;

    fiff_int_t kind = -1;
    fiff_int_t pos = -1;

    for (qint32 k = 0; k < meas_info[0].nent; ++k)
    {
        kind = meas_info[0].dir[k].kind;
        pos  = meas_info[0].dir[k].pos;
        switch (kind)
        {
            case FIFF_NCHAN:
                FiffTag::read_tag(this, t_pTag, pos);
                nchan = *t_pTag->toInt();
                break;
            case FIFF_SFREQ:
                FiffTag::read_tag(this, t_pTag, pos);
                sfreq = *t_pTag->toFloat();
                break;
            case FIFF_CH_INFO:
                FiffTag::read_tag(this, t_pTag, pos);
                chs.append( t_pTag->toChInfo() );
                break;
            case FIFF_LOWPASS:
                FiffTag::read_tag(this, t_pTag, pos);
                lowpass = *t_pTag->toFloat();
                break;
            case FIFF_HIGHPASS:
                FiffTag::read_tag(this, t_pTag, pos);
                highpass = *t_pTag->toFloat();
                break;
            case FIFF_MEAS_DATE:
                FiffTag::read_tag(this, t_pTag, pos);
                meas_date[0] = t_pTag->toInt()[0];
                meas_date[1] = t_pTag->toInt()[1];
                break;
            case FIFF_COORD_TRANS:
                //ToDo: This has to be debugged!!
                FiffTag::read_tag(this, t_pTag, pos);
                cand = t_pTag->toCoordTrans();
                if(cand.from == FIFFV_COORD_DEVICE && cand.to == FIFFV_COORD_HEAD)
                    dev_head_t = cand;
                else if (cand.from == FIFFV_MNE_COORD_CTF_HEAD && cand.to == FIFFV_COORD_HEAD)
                    ctf_head_t = cand;
                break;
        }
    }
    //
    //   Check that we have everything we need
    //
    if (nchan < 0)
    {
        printf("Number of channels in not defined\n");
        return false;
    }
    if (sfreq < 0)
    {
        printf("Sampling frequency is not defined\n");
        return false;
    }
    if (chs.size() == 0)
    {
        printf("Channel information not defined\n");
        return false;
    }
    if (chs.size() != nchan)
    {
        printf("Incorrect number of channel definitions found\n");
        return false;
    }

    if (dev_head_t.isEmpty() || ctf_head_t.isEmpty())
    {
        QList<FiffDirTree> hpi_result = meas_info[0].dir_tree_find(FIFFB_HPI_RESULT);
        if (hpi_result.size() == 1)
        {
            for( qint32 k = 0; k < hpi_result[0].nent; ++k)
            {
                kind = hpi_result[0].dir[k].kind;
                pos  = hpi_result[0].dir[k].pos;
                if (kind == FIFF_COORD_TRANS)
                {
                    FiffTag::read_tag(this, t_pTag, pos);
                    cand = t_pTag->toCoordTrans();
                    if (cand.from == FIFFV_COORD_DEVICE && cand.to == FIFFV_COORD_HEAD)
                        dev_head_t = cand;
                    else if (cand.from == FIFFV_MNE_COORD_CTF_HEAD && cand.to == FIFFV_COORD_HEAD)
                        ctf_head_t = cand;
                }
            }
        }
    }
    //
    //   Locate the Polhemus data
    //
    QList<FiffDirTree> isotrak = meas_info[0].dir_tree_find(FIFFB_ISOTRAK);

    QList<FiffDigPoint> dig;
    fiff_int_t coord_frame = FIFFV_COORD_HEAD;
    FiffCoordTrans dig_trans;
    qint32 k = 0;

    if (isotrak.size() == 1)
    {
        for (k = 0; k < isotrak[0].nent; ++k)
        {
            kind = isotrak[0].dir[k].kind;
            pos  = isotrak[0].dir[k].pos;
            if (kind == FIFF_DIG_POINT)
            {
                FiffTag::read_tag(this, t_pTag, pos);
                dig.append(t_pTag->toDigPoint());
            }
            else
            {
                if (kind == FIFF_MNE_COORD_FRAME)
                {
                    FiffTag::read_tag(this, t_pTag, pos);
                    qDebug() << "NEEDS To BE DEBBUGED: FIFF_MNE_COORD_FRAME" << t_pTag->getType();
                    coord_frame = *t_pTag->toInt();
                }
                else if (kind == FIFF_COORD_TRANS)
                {
                    FiffTag::read_tag(this, t_pTag, pos);
                    qDebug() << "NEEDS To BE DEBBUGED: FIFF_COORD_TRANS" << t_pTag->getType();
                    dig_trans = t_pTag->toCoordTrans();
                }
            }
        }
    }
    for(k = 0; k < dig.size(); ++k)
        dig[k].coord_frame = coord_frame;

    if (!dig_trans.isEmpty()) //if exist('dig_trans','var')
        if (dig_trans.from != coord_frame && dig_trans.to != coord_frame)
            dig_trans.clear();

    //
    //   Locate the acquisition information
    //
    QList<FiffDirTree> acqpars = meas_info[0].dir_tree_find(FIFFB_DACQ_PARS);
    QString acq_pars;
    QString acq_stim;
    if (acqpars.size() == 1)
    {
        for( k = 0; k < acqpars[0].nent; ++k)
        {
            kind = acqpars[0].dir.at(k).kind;
            pos  = acqpars[0].dir.at(k).pos;
            if (kind == FIFF_DACQ_PARS)
            {
                FiffTag::read_tag(this, t_pTag, pos);
                acq_pars = t_pTag->toString();
            }
            else if (kind == FIFF_DACQ_STIM)
            {
                FiffTag::read_tag(this, t_pTag, pos);
                acq_stim = t_pTag->toString();
            }
        }
    }
    //
    //   Load the SSP data
    //
    QList<FiffProj> projs = this->read_proj(meas_info[0]);//ToDo Member Function
    //
    //   Load the CTF compensation data
    //
    QList<FiffCtfComp> comps = this->read_ctf_comp(meas_info[0], chs);//ToDo Member Function
    //
    //   Load the bad channel list
    //
    QStringList bads = this->read_bad_channels(p_Node);
    //
    //   Put the data together
    //
//    info = new FiffInfo();
    if (p_Node.id.version != -1)
        info.file_id = p_Node.id;
    else
        info.file_id.version = -1;

    //
    //  Make the most appropriate selection for the measurement id
    //
    if (meas_info[0].parent_id.version == -1)
    {
        if (meas_info[0].id.version == -1)
        {
            if (meas[0].id.version == -1)
            {
                if (meas[0].parent_id.version == -1)
                    info.meas_id = info.file_id;
                else
                    info.meas_id = meas[0].parent_id;
            }
            else
                info.meas_id = meas[0].id;
        }
        else
            info.meas_id = meas_info[0].id;
    }
    else
        info.meas_id = meas_info[0].parent_id;

    if (meas_date[0] == -1)
    {
        info.meas_date[0] = info.meas_id.time.secs;
        info.meas_date[1] = info.meas_id.time.usecs;
    }
    else
    {
        info.meas_date[0] = meas_date[0];
        info.meas_date[1] = meas_date[1];
    }

    info.nchan  = nchan;
    info.sfreq  = sfreq;
    if (highpass != -1.0f)
        info.highpass = highpass;
    else
        info.highpass = 0.0f;

    if (lowpass != -1.0f)
        info.lowpass = lowpass;
    else
        info.lowpass = info.sfreq/2.0;

    //
    //   Add the channel information and make a list of channel names
    //   for convenience
    //
    info.chs = chs;
    for (qint32 c = 0; c < info.nchan; ++c)
        info.ch_names << info.chs[c].ch_name;

    //
    //  Add the coordinate transformations
    //
    info.dev_head_t = dev_head_t;
    info.ctf_head_t = ctf_head_t;
    if ((!info.dev_head_t.isEmpty()) && (!info.ctf_head_t.isEmpty())) //~isempty(info.dev_head_t) && ~isempty(info.ctf_head_t)
    {
        info.dev_ctf_t     = info.dev_head_t;
        info.dev_ctf_t.to = info.ctf_head_t.from;
        info.dev_ctf_t.trans = ctf_head_t.trans.inverse()*info.dev_ctf_t.trans;
    }
    else
        info.dev_ctf_t.clear();

    //
    //   All kinds of auxliary stuff
    //
    info.dig   = dig;
    if (!dig_trans.isEmpty())
        info.dig_trans = dig_trans;

    info.bads  = bads;
    info.projs = projs;
    info.comps = comps;
    info.acq_pars = acq_pars;
    info.acq_stim = acq_stim;

    p_NodeInfo = meas[0];

    //Garbage Collecting;
    if(t_pTag)
        delete t_pTag;
    return true;
}


//*************************************************************************************************************

bool FiffStream::read_named_matrix(const FiffDirTree& p_Node, fiff_int_t matkind, FiffNamedMatrix& mat)
{
    mat.clear();

    FiffDirTree node = p_Node;
    //
    //   Descend one level if necessary
    //
    bool found_it = false;
    if (node.block != FIFFB_MNE_NAMED_MATRIX)
    {
        for (int k = 0; k < node.nchild; ++k)
        {
            if (node.children[k].block == FIFFB_MNE_NAMED_MATRIX)
            {
                if(node.children[k].has_tag(matkind))
                {
                    node = node.children[k];
                    found_it = true;
                    break;
                }
            }
       }
       if (!found_it)
       {
          printf("Fiff::read_named_matrix: Desired named matrix (kind = %d) not available\n",matkind);
          return false;
       }
    }
    else
    {
        if (!node.has_tag(matkind))
        {
            printf("Desired named matrix (kind = %d) not available",matkind);
            return false;
        }
    }

    FIFFLIB::FiffTag* t_pTag = NULL;
    //
    //   Read everything we need
    //
    if(!node.find_tag(this, matkind, t_pTag))
    {
        printf("Matrix data missing.\n");
        return false;
    }
    else
    {
        //qDebug() << "Is Matrix" << t_pTag->isMatrix() << "Special Type:" << t_pTag->getType();
        mat.data = t_pTag->toFloatMatrix();
        mat.data.transposeInPlace();
    }

    mat.nrow = mat.data.rows();
    mat.ncol = mat.data.cols();

    if(node.find_tag(this, FIFF_MNE_NROW, t_pTag))
        if (*t_pTag->toInt() != mat.nrow)
        {
            printf("Number of rows in matrix data and FIFF_MNE_NROW tag do not match");
            return false;
        }
    if(node.find_tag(this, FIFF_MNE_NCOL, t_pTag))
        if (*t_pTag->toInt() != mat.ncol)
        {
            printf("Number of columns in matrix data and FIFF_MNE_NCOL tag do not match");
            return false;
        }

    QString row_names;
    if(node.find_tag(this, FIFF_MNE_ROW_NAMES, t_pTag))
        row_names = t_pTag->toString();

    QString col_names;
    if(node.find_tag(this, FIFF_MNE_COL_NAMES, t_pTag))
        col_names = t_pTag->toString();

    //
    //   Put it together
    //
    if (!row_names.isEmpty())
        mat.row_names = split_name_list(row_names);

    if (!col_names.isEmpty())
        mat.col_names = split_name_list(col_names);

    return true;
}


//*************************************************************************************************************

QList<FiffProj> FiffStream::read_proj(const FiffDirTree& p_Node)
{
    QList<FiffProj> projdata;// = struct('kind',{},'active',{},'desc',{},'data',{});
    //
    //   Locate the projection data
    //
    QList<FiffDirTree> t_qListNodes = p_Node.dir_tree_find(FIFFB_PROJ);
    if ( t_qListNodes.size() == 0 )
        return projdata;


    FIFFLIB::FiffTag* t_pTag = NULL;
    t_qListNodes[0].find_tag(this, FIFF_NCHAN, t_pTag);
    fiff_int_t global_nchan;
    if (t_pTag)
        global_nchan = *t_pTag->toInt();


    fiff_int_t nchan;
    QList<FiffDirTree> t_qListItems = t_qListNodes[0].dir_tree_find(FIFFB_PROJ_ITEM);
    for ( qint32 i = 0; i < t_qListItems.size(); ++i)
    {
        //
        //   Find all desired tags in one item
        //
        FiffDirTree* t_pFiffDirTreeItem = &t_qListItems[i];
        t_pFiffDirTreeItem->find_tag(this, FIFF_NCHAN, t_pTag);
        if (t_pTag)
            nchan = *t_pTag->toInt();
        else
            nchan = global_nchan;

        t_pFiffDirTreeItem->find_tag(this, FIFF_DESCRIPTION, t_pTag);
        QString desc; // maybe, in some cases this has to be a struct.
        if (t_pTag)
        {
            qDebug() << "read_proj: this has to be debugged";
            desc = t_pTag->toString();
        }
        else
        {
            t_pFiffDirTreeItem->find_tag(this, FIFF_NAME, t_pTag);
            if (t_pTag)
                desc = t_pTag->toString();
            else
            {
                printf("Projection item description missing\n");
                return projdata;
            }
        }
//            t_pFiffDirTreeItem->find_tag(this, FIFF_PROJ_ITEM_CH_NAME_LIST, t_pTag);
//            QString namelist;
//            if (t_pTag)
//            {
//                namelist = t_pTag->toString();
//            }
//            else
//            {
//                printf("Projection item channel list missing\n");
//                return projdata;
//            }
        t_pFiffDirTreeItem->find_tag(this, FIFF_PROJ_ITEM_KIND, t_pTag);
        fiff_int_t kind;
        if (t_pTag)
        {
            kind = *t_pTag->toInt();
        }
        else
        {
            printf("Projection item kind missing");
            return projdata;
        }
        t_pFiffDirTreeItem->find_tag(this, FIFF_PROJ_ITEM_NVEC, t_pTag);
        fiff_int_t nvec;
        if (t_pTag)
        {
            nvec = *t_pTag->toInt();
        }
        else
        {
            printf("Number of projection vectors not specified\n");
            return projdata;
        }
        t_pFiffDirTreeItem->find_tag(this, FIFF_PROJ_ITEM_CH_NAME_LIST, t_pTag);
        QStringList names;
        if (t_pTag)
        {
            names = split_name_list(t_pTag->toString());
        }
        else
        {
            printf("Projection item channel list missing\n");
            return projdata;
        }
        t_pFiffDirTreeItem->find_tag(this, FIFF_PROJ_ITEM_VECTORS, t_pTag);
        MatrixXd data;// = NULL;
        if (t_pTag)
        {
            data = t_pTag->toFloatMatrix();
            data.transposeInPlace();
        }
        else
        {
            printf("Projection item data missing\n");
            return projdata;
        }
        t_pFiffDirTreeItem->find_tag(this, FIFF_MNE_PROJ_ITEM_ACTIVE, t_pTag);
        bool active;
        if (t_pTag)
            active = *t_pTag->toInt();
        else
            active = false;

        if (data.cols() != names.size())
        {
            printf("Number of channel names does not match the size of data matrix\n");
            return projdata;
        }



        //
        //   create a named matrix for the data
        //
        QStringList defaultList;
        FiffNamedMatrix t_fiffNamedMatrix(nvec, nchan, defaultList, names, data);

        FiffProj one(kind, active, desc, t_fiffNamedMatrix);
        //
        projdata.append(one);
    }

    if (projdata.size() > 0)
    {
        printf("\tRead a total of %d projection items:\n", projdata.size());
        for(qint32 k = 0; k < projdata.size(); ++k)
        {
            printf("\t\t%s (%d x %d)",projdata[k].desc.toUtf8().constData(), projdata[k].data->nrow, projdata[k].data->ncol);
            if (projdata[k].active)
                printf(" active\n");
            else
                printf(" idle\n");
        }
    }

    //Garbage Collection
    if (t_pTag)
        delete t_pTag;

    return projdata;
}


//*************************************************************************************************************

bool FiffStream::setup_read_raw(QIODevice &p_IODevice, FiffRawData& data, bool allow_maxshield)
{
    //
    //   Open the file
    //
    FiffStream::SPtr p_pStream = FiffStream::SPtr(new FiffStream(&p_IODevice));
    QString t_sFileName = p_pStream->streamName();

    printf("Opening raw data %s...\n",t_sFileName.toUtf8().constData());

    FiffDirTree t_Tree;
    QList<FiffDirEntry> t_Dir;

    if(!p_pStream->open(t_Tree, t_Dir))
        return false;

    //
    //   Read the measurement info
    //
    FiffInfo info;// = NULL;
    FiffDirTree meas;
    if(!p_pStream->read_meas_info(t_Tree, info, meas))
        return false;

    //
    //   Locate the data of interest
    //
    QList<FiffDirTree> raw = meas.dir_tree_find(FIFFB_RAW_DATA);
    if (raw.size() == 0)
    {
        raw = meas.dir_tree_find(FIFFB_CONTINUOUS_DATA);
        if(allow_maxshield)
        {
//            for (qint32 i = 0; i < raw.size(); ++i)
//                if(raw[i])
//                    delete raw[i];
            raw = meas.dir_tree_find(FIFFB_SMSH_RAW_DATA);
            if (raw.size() == 0)
            {
                printf("No raw data in %s\n", t_sFileName.toUtf8().constData());
                return false;
            }
        }
        else
        {
            if (raw.size() == 0)
            {
                printf("No raw data in %s\n", t_sFileName.toUtf8().constData());
                return false;
            }
        }
    }
    //
    //   Set up the output structure
    //
    info.filename   = t_sFileName;

    data.clear();
    data.file = p_pStream;// fid;
    data.info = info;
    data.first_samp = 0;
    data.last_samp  = 0;
    //
    //   Process the directory
    //

    QList<FiffDirEntry> dir = raw[0].dir;
    fiff_int_t nent = raw[0].nent;
    fiff_int_t nchan = info.nchan;
    fiff_int_t first = 0;
    fiff_int_t first_samp = 0;
    fiff_int_t first_skip = 0;
    //
    //  Get first sample tag if it is there
    //
    FiffTag* t_pTag = NULL;
    if (dir[first].kind == FIFF_FIRST_SAMPLE)
    {
        FiffTag::read_tag(p_pStream.data(), t_pTag, dir[first].pos);
        first_samp = *t_pTag->toInt();
        ++first;
    }

    //
    //  Omit initial skip
    //
    if (dir.at(first).kind == FIFF_DATA_SKIP)
    {
        //
        //  This first skip can be applied only after we know the buffer size
        //
        FiffTag::read_tag(p_pStream.data(), t_pTag, dir[first].pos);
        first_skip = *t_pTag->toInt();
        ++first;
    }
    data.first_samp = first_samp;
    //
    //   Go through the remaining tags in the directory
    //
    QList<FiffRawDir> rawdir;
//        rawdir = struct('ent',{},'first',{},'last',{},'nsamp',{});
    fiff_int_t nskip = 0;
    fiff_int_t ndir  = 0;
    fiff_int_t nsamp = 0;
    for (qint32 k = first; k < nent; ++k)
    {
        FiffDirEntry ent = dir.at(k);
        if (ent.kind == FIFF_DATA_SKIP)
        {
            FiffTag::read_tag(p_pStream.data(), t_pTag, ent.pos);
            nskip = *t_pTag->toInt();
        }
        else if(ent.kind == FIFF_DATA_BUFFER)
        {
            //
            //   Figure out the number of samples in this buffer
            //
            switch(ent.type)
            {
                case FIFFT_DAU_PACK16:
                    nsamp = ent.size/(2*nchan);
                    break;
                case FIFFT_SHORT:
                    nsamp = ent.size/(2*nchan);
                    break;
                case FIFFT_FLOAT:
                    nsamp = ent.size/(4*nchan);
                    break;
                case FIFFT_INT:
                    nsamp = ent.size/(4*nchan);
                    break;
                default:
                    printf("Cannot handle data buffers of type %d\n",ent.type);
                    return false;
            }
            //
            //  Do we have an initial skip pending?
            //
            if (first_skip > 0)
            {
                first_samp += nsamp*first_skip;
                data.first_samp = first_samp;
                first_skip = 0;
            }
            //
            //  Do we have a skip pending?
            //
            if (nskip > 0)
            {
                FiffRawDir t_RawDir;
                t_RawDir.first = first_samp;
                t_RawDir.last  = first_samp + nskip*nsamp - 1;//ToDo -1 right or is that MATLAB syntax
                t_RawDir.nsamp = nskip*nsamp;
                rawdir.append(t_RawDir);
                first_samp = first_samp + nskip*nsamp;
                nskip = 0;
                ++ndir;
            }
            //
            //  Add a data buffer
            //
            FiffRawDir t_RawDir;
            t_RawDir.ent   = ent;
            t_RawDir.first = first_samp;
            t_RawDir.last  = first_samp + nsamp - 1;//ToDo -1 right or is that MATLAB syntax
            t_RawDir.nsamp = nsamp;
            rawdir.append(t_RawDir);
            first_samp += nsamp;
            ++ndir;
        }
    }
    data.last_samp  = first_samp - 1;//ToDo -1 right or is that MATLAB syntax
    //
    //   Add the calibration factors
    //
    MatrixXd cals(1,data.info.nchan);
    cals.setZero();
    for (qint32 k = 0; k < data.info.nchan; ++k)
        cals(0,k) = data.info.chs.at(k).range*data.info.chs[k].cal;
    //
    data.cals       = cals;
    data.rawdir     = rawdir;
    //data->proj       = [];
    //data.comp       = [];
    //
    printf("\tRange : %d ... %d  =  %9.3f ... %9.3f secs\n",
           data.first_samp,data.last_samp,
           (double)data.first_samp/data.info.sfreq,
           (double)data.last_samp/data.info.sfreq);
    printf("Ready.\n");
    data.file->device()->close();

    return true;
}


//*************************************************************************************************************

QStringList FiffStream::split_name_list(QString p_sNameList)
{
    return p_sNameList.split(":");
}

//*************************************************************************************************************

void FiffStream::start_block(fiff_int_t kind)
{
    this->write_int(FIFF_BLOCK_START,&kind);
}


//*************************************************************************************************************

FiffStream* FiffStream::start_file(QIODevice* p_pIODevice)
{
    FiffStream* p_pStream = new FiffStream(p_pIODevice);
    QString t_sFileName = p_pStream->streamName();

    if(!p_pStream->device()->open(QIODevice::WriteOnly))
    {
        printf("Cannot write to %s\n", t_sFileName.toUtf8().constData());//consider throw
        delete p_pStream;
        return NULL;
    }

    //
    //   Write the compulsory items
    //
    p_pStream->write_id(FIFF_FILE_ID);//1
    qint32 data = -1;
    p_pStream->write_int(FIFF_DIR_POINTER,&data);//2
    p_pStream->write_int(FIFF_FREE_LIST,&data);//3
    //
    //   Ready for more
    //
    return p_pStream;
}


//*************************************************************************************************************

FiffStream* FiffStream::start_writing_raw(QIODevice* p_pIODevice, const FiffInfo& info, MatrixXd& cals, MatrixXi sel)
{
    //
    //   We will always write floats
    //
    fiff_int_t data_type = 4;
    qint32 k;

    if(sel.cols() == 0)
    {
        sel.resize(1,info.nchan);
        for (k = 0; k < info.nchan; ++k)
            sel(0, k) = k; //+1 when MATLAB notation
    }

    QList<FiffChInfo> chs;

    for(k = 0; k < sel.cols(); ++k)
        chs << info.chs.at(sel(0,k));

    fiff_int_t nchan = chs.size();
    //
    //  Create the file and save the essentials
    //

    FiffStream* t_pStream = start_file(p_pIODevice);//1, 2, 3
    t_pStream->start_block(FIFFB_MEAS);//4
    t_pStream->write_id(FIFF_BLOCK_ID);//5
    if(info.meas_id.version != -1)
    {
        t_pStream->write_id(FIFF_PARENT_BLOCK_ID,info.meas_id);//6
    }
    //
    //
    //    Measurement info
    //
    t_pStream->start_block(FIFFB_MEAS_INFO);//7
    //
    //    Blocks from the original
    //
    QList<fiff_int_t> blocks;
    blocks << FIFFB_SUBJECT << FIFFB_HPI_MEAS << FIFFB_HPI_RESULT << FIFFB_ISOTRAK << FIFFB_PROCESSING_HISTORY;
    bool have_hpi_result = false;
    bool have_isotrak    = false;
    if (blocks.size() > 0 && !info.filename.isEmpty())
    {
        QFile* t_pFile = new QFile(info.filename);//ToDo this has to be adapted for TCPSocket
        FiffStream* t_pStream2 = new FiffStream(t_pFile);

        FiffDirTree t_Tree;
        QList<FiffDirEntry> t_Dir;
        t_pStream2->open(t_Tree, t_Dir);

        for(qint32 k = 0; k < blocks.size(); ++k)
        {
            QList<FiffDirTree> nodes = t_Tree.dir_tree_find(blocks[k]);
            FiffDirTree::copy_tree(t_pStream2,t_Tree.id,nodes,t_pStream);
            if(blocks[k] == FIFFB_HPI_RESULT && nodes.size() > 0)
                have_hpi_result = true;

            if(blocks[k] == FIFFB_ISOTRAK && nodes.size() > 0)
                have_isotrak = true;
        }

        delete t_pStream2;
        t_pStream2 = NULL;
    }
    //
    //    megacq parameters
    //
    if (!info.acq_pars.isEmpty() || !info.acq_stim.isEmpty())
    {
        t_pStream->start_block(FIFFB_DACQ_PARS);
        if (!info.acq_pars.isEmpty())
            t_pStream->write_string(FIFF_DACQ_PARS, info.acq_pars);

        if (!info.acq_stim.isEmpty())
            t_pStream->write_string(FIFF_DACQ_STIM, info.acq_stim);

        t_pStream->end_block(FIFFB_DACQ_PARS);
    }
    //
    //    Coordinate transformations if the HPI result block was not there
    //
    if (!have_hpi_result)
    {
        if (!info.dev_head_t.isEmpty())
            t_pStream->write_coord_trans(info.dev_head_t);

        if (!info.ctf_head_t.isEmpty())
            t_pStream->write_coord_trans(info.ctf_head_t);
    }
    //
    //    Polhemus data
    //
    if (info.dig.size() > 0 && !have_isotrak)
    {
        t_pStream->start_block(FIFFB_ISOTRAK);
        for (qint32 k = 0; k < info.dig.size(); ++k)
            t_pStream->write_dig_point(info.dig[k]);

        t_pStream->end_block(FIFFB_ISOTRAK);
    }
    //
    //    Projectors
    //
    t_pStream->write_proj(info.projs);
    //
    //    CTF compensation info
    //
    t_pStream->write_ctf_comp(info.comps);
    //
    //    Bad channels
    //
    if (info.bads.size() > 0)
    {
        t_pStream->start_block(FIFFB_MNE_BAD_CHANNELS);
        t_pStream->write_name_list(FIFF_MNE_CH_NAME_LIST,info.bads);
        t_pStream->end_block(FIFFB_MNE_BAD_CHANNELS);
    }
    //
    //    General
    //
    t_pStream->write_float(FIFF_SFREQ,&info.sfreq);
    t_pStream->write_float(FIFF_HIGHPASS,&info.highpass);
    t_pStream->write_float(FIFF_LOWPASS,&info.lowpass);
    t_pStream->write_int(FIFF_NCHAN,&nchan);
    t_pStream->write_int(FIFF_DATA_PACK,&data_type);
    if (info.meas_date[0] != -1)
        t_pStream->write_int(FIFF_MEAS_DATE,info.meas_date, 2);
    //
    //    Channel info
    //
    cals = MatrixXd(1,nchan);

    for(k = 0; k < nchan; ++k)
    {
        //
        //    Scan numbers may have been messed up
        //
        chs[k].scanno = k+1;//+1 because
        chs[k].range  = 1.0f;//Why? -> cause its already calibrated through reading
        cals(0,k) = chs[k].cal;
        t_pStream->write_ch_info(&chs[k]);
    }
    //
    //
    t_pStream->end_block(FIFFB_MEAS_INFO);
    //
    // Start the raw data
    //
    t_pStream->start_block(FIFFB_RAW_DATA);

    return t_pStream;
}


//*************************************************************************************************************

QString FiffStream::streamName()
{
    QFile* t_pFile = qobject_cast<QFile*>(this->device());
    QString p_sFileName;
    if(t_pFile)
        p_sFileName = t_pFile->fileName();
    else
        p_sFileName = QString("TCPSocket");

    return p_sFileName;
}


//*************************************************************************************************************

void FiffStream::write_ch_info(FiffChInfo* ch)
{
    //typedef struct _fiffChPosRec {
    //  fiff_int_t   coil_type;          /*!< What kind of coil. */
    //  fiff_float_t r0[3];              /*!< Coil coordinate system origin */
    //  fiff_float_t ex[3];              /*!< Coil coordinate system x-axis unit vector */
    //  fiff_float_t ey[3];              /*!< Coil coordinate system y-axis unit vector */
    //  fiff_float_t ez[3];             /*!< Coil coordinate system z-axis unit vector */
    //} fiffChPosRec,*fiffChPos;        /*!< Measurement channel position and coil type */


    //typedef struct _fiffChInfoRec {
    //  fiff_int_t    scanNo;        /*!< Scanning order # */
    //  fiff_int_t    logNo;         /*!< Logical channel # */
    //  fiff_int_t    kind;          /*!< Kind of channel */
    //  fiff_float_t  range;         /*!< Voltmeter range (only applies to raw data ) */
    //  fiff_float_t  cal;           /*!< Calibration from volts to... */
    //  fiff_ch_pos_t chpos;         /*!< Channel location */
    //  fiff_int_t    unit;          /*!< Unit of measurement */
    //  fiff_int_t    unit_mul;      /*!< Unit multiplier exponent */
    //  fiff_char_t   ch_name[16];   /*!< Descriptive name for the channel */
    //} fiffChInfoRec,*fiffChInfo;   /*!< Description of one channel */
    fiff_int_t datasize= 4*13 + 4*7 + 16;

    *this << (qint32)FIFF_CH_INFO;
    *this << (qint32)FIFFT_CH_INFO_STRUCT;
    *this << (qint32)datasize;
    *this << (qint32)FIFFV_NEXT_SEQ;

    //
    //   Start writing fiffChInfoRec
    //
    *this << (qint32)ch->scanno;
    *this << (qint32)ch->logno;
    *this << (qint32)ch->kind;

    *this << ch->range;
    *this << ch->cal;

    //
    //   fiffChPosRec follows
    //
    *this << (qint32)ch->coil_type;
    qint32 i;
    for(i = 0; i < 12; ++i)
        *this << ch->loc(i,0);

    //
    //   unit and unit multiplier
    //
    *this << (qint32)ch->unit;
    *this << (qint32)ch->unit_mul;

    //
    //   Finally channel name
    //
    fiff_int_t len = ch->ch_name.size();
    QString ch_name;
    if(len > 15)
        ch_name = ch->ch_name.mid(0, 15);
    else
        ch_name = ch->ch_name;

    len = ch_name.size();

    this->writeRawData(ch_name.toUtf8().constData(),len);

    if (len < 16)
    {
        const char* chNull = "";
        for(i = 0; i < 16-len; ++i)
            this->writeRawData(chNull,1);
    }
}


//*************************************************************************************************************

void FiffStream::write_coord_trans(const FiffCoordTrans& trans)
{
    //?typedef struct _fiffCoordTransRec {
    //  fiff_int_t   from;                   /*!< Source coordinate system. */
    //  fiff_int_t   to;                     /*!< Destination coordinate system. */
    //  fiff_float_t rot[3][3];              /*!< The forward transform (rotation part) */
    //  fiff_float_t move[3];                /*!< The forward transform (translation part) */
    //  fiff_float_t invrot[3][3];           /*!< The inverse transform (rotation part) */
    //  fiff_float_t invmove[3];             /*!< The inverse transform (translation part) */
    //} *fiffCoordTrans, fiffCoordTransRec;  /*!< Coordinate transformation descriptor */
    fiff_int_t datasize = 4*2*12 + 4*2;

    *this << (qint32)FIFF_COORD_TRANS;
    *this << (qint32)FIFFT_COORD_TRANS_STRUCT;
    *this << (qint32)datasize;
    *this << (qint32)FIFFV_NEXT_SEQ;

    //
    //   Start writing fiffCoordTransRec
    //
    *this << (qint32)trans.from;
    *this << (qint32)trans.to;

    //
    //   The transform...
    //
    qint32 r, c;
    for (r = 0; r < 3; ++r)
        for (c = 0; c < 3; ++c)
            *this << (float)trans.trans(r,c);
    for (r = 0; r < 3; ++r)
        *this << (float)trans.trans(r,3);

    //
    //   ...and its inverse
    //
    for (r = 0; r < 3; ++r)
        for (c = 0; c < 3; ++c)
            *this << (float)trans.invtrans(r,c);
    for (r = 0; r < 3; ++r)
        *this << (float)trans.invtrans(r,3);
}


//*************************************************************************************************************

void FiffStream::write_ctf_comp(const QList<FiffCtfComp>& comps)
{
    if (comps.size() <= 0)
        return;
    //
    //  This is very simple in fact
    //
    this->start_block(FIFFB_MNE_CTF_COMP);
    for(qint32 k = 0; k < comps.size(); ++k)
    {
        FiffCtfComp comp(comps[k]);
        this->start_block(FIFFB_MNE_CTF_COMP_DATA);
        //
        //    Write the compensation kind
        //
        this->write_int(FIFF_MNE_CTF_COMP_KIND, &comp.ctfkind);
        qint32 save_calibrated = comp.save_calibrated;
        this->write_int(FIFF_MNE_CTF_COMP_CALIBRATED, &save_calibrated);
        //
        //    Write an uncalibrated or calibrated matrix
        //
        comp.data->data = (comp.rowcals.diagonal()).inverse()* comp.data->data * (comp.colcals.diagonal()).inverse();
        this->write_named_matrix(FIFF_MNE_CTF_COMP_DATA,*comp.data.data());
        this->end_block(FIFFB_MNE_CTF_COMP_DATA);
    }
    this->end_block(FIFFB_MNE_CTF_COMP);

    return;
}


//*************************************************************************************************************

void FiffStream::write_dig_point(const FiffDigPoint& dig)
{
    //?typedef struct _fiffDigPointRec {
    //  fiff_int_t kind;               /*!< FIFF_POINT_CARDINAL,
    //                                  *   FIFF_POINT_HPI, or
    //                                  *   FIFF_POINT_EEG */
    //  fiff_int_t ident;              /*!< Number identifying this point */
    //  fiff_float_t r[3];             /*!< Point location */
    //} *fiffDigPoint,fiffDigPointRec; /*!< Digitization point description */
    fiff_int_t datasize = 5*4;

    *this << (qint32)FIFF_DIG_POINT;
    *this << (qint32)FIFFT_DIG_POINT_STRUCT;
    *this << (qint32)datasize;
    *this << (qint32)FIFFV_NEXT_SEQ;

    //
    //   Start writing fiffDigPointRec
    //
    *this << (qint32)dig.kind;
    *this << (qint32)dig.ident;
    for(qint32 i = 0; i < 3; ++i)
        *this << dig.r[i];
}


//*************************************************************************************************************

void FiffStream::write_float(fiff_int_t kind, const float* data, fiff_int_t nel)
{
    qint32 datasize = nel * 4;

    *this << (qint32)kind;
    *this << (qint32)FIFFT_FLOAT;
    *this << (qint32)datasize;
    *this << (qint32)FIFFV_NEXT_SEQ;

//    this->setFloatingPointPrecision(QDataStream::SinglePrecision);

    for(qint32 i = 0; i < nel; ++i)
        *this << data[i];
}


//*************************************************************************************************************

void FiffStream::write_float_matrix(fiff_int_t kind, const MatrixXd& mat)
{
    qint32 FIFFT_MATRIX = 1 << 30;
    qint32 FIFFT_MATRIX_FLOAT = FIFFT_FLOAT | FIFFT_MATRIX;

    qint32 numel = mat.rows() * mat.cols();

    fiff_int_t datasize = 4*numel + 4*3;

    *this << (qint32)kind;
    *this << (qint32)FIFFT_MATRIX_FLOAT;
    *this << (qint32)datasize;
    *this << (qint32)FIFFV_NEXT_SEQ;

    qint32 i;
    for(i = 0; i < numel; ++i)
        *this << (float)mat.data()[i];

    qint32 dims[3];
    dims[0] = mat.cols();
    dims[1] = mat.rows();
    dims[2] = 2;

    for(i = 0; i < 3; ++i)
        *this << dims[i];
}


//*************************************************************************************************************

void FiffStream::write_id(fiff_int_t kind, const FiffId& id)
{
    FiffId t_id = id;
    if(t_id.version == -1)
    {
        /* initialize random seed: */
        srand ( time(NULL) );
        double rand_1 = (double)(rand() % 100);rand_1 /= 100;
        double rand_2 = (double)(rand() % 100);rand_2 /= 100;

        time_t seconds;
        seconds = time (NULL);

        //fiff_int_t timezone = 5;      //   Matlab does not know the timezone
        t_id.version   = (1 << 16) | 2;   //   Version (1 << 16) | 2
        t_id.machid[0] = 65536*rand_1;    //   Machine id is random for now
        t_id.machid[1] = 65536*rand_2;    //   Machine id is random for now
        t_id.time.secs = (int)seconds;    //seconds since January 1, 1970 //3600*(24*(now-datenum(1970,1,1,0,0,0))+timezone);
        t_id.time.usecs = 0;              //   Do not know how we could get this
    }

    //
    //
    fiff_int_t datasize = 5*4;                       //   The id comprises five integers

    *this << (qint32)kind;
    *this << (qint32)FIFFT_ID_STRUCT;
    *this << (qint32)datasize;
    *this << (qint32)FIFFV_NEXT_SEQ;
    //
    // Collect the bits together for one write
    //
    qint32 data[5];
    data[0] = t_id.version;
    data[1] = t_id.machid[0];
    data[2] = t_id.machid[1];
    data[3] = t_id.time.secs;
    data[4] = t_id.time.usecs;

    for(qint32 i = 0; i < 5; ++i)
        *this << data[i];
}


//*************************************************************************************************************

void FiffStream::write_int(fiff_int_t kind, const fiff_int_t* data, fiff_int_t nel)
{
    fiff_int_t datasize = nel * 4;

    *this << (qint32)kind;
    *this << (qint32)FIFFT_INT;
    *this << (qint32)datasize;
    *this << (qint32)FIFFV_NEXT_SEQ;

    for(qint32 i = 0; i < nel; ++i)
        *this << data[i];
}


//*************************************************************************************************************

void FiffStream::write_name_list(fiff_int_t kind, const QStringList& data)
{
    QString all = data.join(":");
    this->write_string(kind,all);
}


//*************************************************************************************************************

void FiffStream::write_named_matrix(fiff_int_t kind, const FiffNamedMatrix& mat)
{
    this->start_block(FIFFB_MNE_NAMED_MATRIX);
    this->write_int(FIFF_MNE_NROW, &mat.nrow);
    this->write_int(FIFF_MNE_NCOL, &mat.ncol);
    if (mat.row_names.size() > 0)
       this->write_name_list(FIFF_MNE_ROW_NAMES, mat.row_names);
    if (mat.col_names.size() > 0)
       this->write_name_list(FIFF_MNE_COL_NAMES, mat.col_names);
    this->write_float_matrix(kind,mat.data);
    this->end_block(FIFFB_MNE_NAMED_MATRIX);
}


//*************************************************************************************************************

void FiffStream::write_proj(const QList<FiffProj>& projs)
{
    if (projs.size() <= 0)
        return;

    this->start_block(FIFFB_PROJ);

    for(qint32 k = 0; k < projs.size(); ++k)
    {
        this->start_block(FIFFB_PROJ_ITEM);
        this->write_string(FIFF_NAME,projs[k].desc);
        this->write_int(FIFF_PROJ_ITEM_KIND,&projs[k].kind);
        if (projs[k].kind == FIFFV_PROJ_ITEM_FIELD)
        {
            float fValue = 0.0f;
            this->write_float(FIFF_PROJ_ITEM_TIME, &fValue);
        }

        this->write_int(FIFF_NCHAN, &projs[k].data->ncol);
        this->write_int(FIFF_PROJ_ITEM_NVEC, &projs[k].data->nrow);
        qint32 bValue = (qint32)projs[k].active;
        this->write_int(FIFF_MNE_PROJ_ITEM_ACTIVE, &bValue);
        this->write_name_list(FIFF_PROJ_ITEM_CH_NAME_LIST, projs[k].data->col_names);
        this->write_float_matrix(FIFF_PROJ_ITEM_VECTORS, projs[k].data->data);
        this->end_block(FIFFB_PROJ_ITEM);
    }
    this->end_block(FIFFB_PROJ);
}


//*************************************************************************************************************

bool FiffStream::write_raw_buffer(const MatrixXd& buf, const MatrixXd& cals)
{
    if (buf.rows() != cals.cols())
    {
        printf("buffer and calibration sizes do not match\n");
        return false;
    }

    SparseMatrix<double> inv_calsMat(cals.cols(), cals.cols());

    for(qint32 i = 0; i < cals.cols(); ++i)
        inv_calsMat.insert(i, i) = 1.0/cals(0,i);

    MatrixXf tmp = (inv_calsMat*buf).cast<float>();
    this->write_float(FIFF_DATA_BUFFER,tmp.data(),tmp.rows()*tmp.cols());
    return true;
}


//*************************************************************************************************************

void FiffStream::write_string(fiff_int_t kind, const QString& data)
{
    fiff_int_t datasize = data.size();
    *this << (qint32)kind;
    *this << (qint32)FIFFT_STRING;
    *this << (qint32)datasize;
    *this << (qint32)FIFFV_NEXT_SEQ;

    this->writeRawData(data.toUtf8().constData(),datasize);
}


//*************************************************************************************************************

void FiffStream::write_rt_command(fiff_int_t command, const QString& data)
{
    fiff_int_t datasize = data.size();
    *this << (qint32)FIFF_MNE_RT_COMMAND;
    *this << (qint32)FIFFT_VOID;
    *this << 4+(qint32)datasize;
    *this << (qint32)FIFFV_NEXT_SEQ;
    *this << command;

    this->writeRawData(data.toUtf8().constData(),datasize);
}
