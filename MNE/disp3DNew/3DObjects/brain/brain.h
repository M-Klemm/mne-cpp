//=============================================================================================================
/**
* @file     brain.h
* @author   Lorenz Esch <Lorenz.Esch@tu-ilmenau.de>;
*           Matti Hamalainen <msh@nmr.mgh.harvard.edu>
* @version  1.0
* @date     November, 2015
*
* @section  LICENSE
*
* Copyright (C) 2015, Lorenz Esch and Matti Hamalainen. All rights reserved.
*
* Redistribution and use in source and binary forms, with or without modification, are permitted provided that
* the following conditions are met:
*     * Redistributions of source code must retain the above copyright notice, this list of conditions and the
*       following disclaimer.
*     * Redistributions in binary form must reproduce the above copyright notice, this list of conditions and
*       the following disclaimer in the documentation and/or other materials provided with the distribution.
*     * Neither the name of MNE-CPP authors nor the names of its contributors may be used
*       to endorse or promote products derived from this software without specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED
* WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
* PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
* INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
* PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
* HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
* NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
* POSSIBILITY OF SUCH DAMAGE.
*
*
* @brief    Brain class declaration
*
*/

#ifndef BRAIN_H
#define BRAIN_H

//*************************************************************************************************************
//=============================================================================================================
// INCLUDES
//=============================================================================================================

#include "disp3dnew_global.h"

#include "braintreemodel.h"

#include <fs/surfaceset.h>
#include <fs/annotationset.h>

#include <disp/helpers/colormap.h>

#include "../../rt/sourcelevel/stcdatamodel.h"


//*************************************************************************************************************
//=============================================================================================================
// QT INCLUDES
//=============================================================================================================

#include <QSharedPointer>

#include <Qt3DCore/QEntity>


//*************************************************************************************************************
//=============================================================================================================
// FORWARD DECLARATIONS
//=============================================================================================================


//*************************************************************************************************************
//=============================================================================================================
// DEFINE NAMESPACE DISP3DNEWLIB
//=============================================================================================================

namespace DISP3DNEWLIB
{

//*************************************************************************************************************
//=============================================================================================================
// USED NAMESPACES
//=============================================================================================================

using namespace FSLIB;

//*************************************************************************************************************
//=============================================================================================================
// FORWARD DECLARATIONS
//=============================================================================================================


//=============================================================================================================
/**
* Visualizes a brain in 3D.
*
* @brief Visualizes a brain in 3D.
*/
class DISP3DNEWSHARED_EXPORT Brain : public Qt3DCore::QEntity
{
    Q_OBJECT

public:
    typedef QSharedPointer<Brain> SPtr;             /**< Shared pointer type for Brain class. */
    typedef QSharedPointer<const Brain> ConstSPtr;  /**< Const shared pointer type for Brain class. */

    //=========================================================================================================
    /**
    * Default constructor.
    *
    * @param[in] parent         The parent of this class.
    */
    Brain(QEntity * parent = 0);

    //=========================================================================================================
    /**
    * Default destructor.
    */
    ~Brain();

    //=========================================================================================================
    /**
    * Adds FreeSurfer brain data SETS.
    *
    * @param[in] tSurfaceSet        FreeSurfer surface set.
    * @param[in] tAnnotationSet     FreeSurfer annotation set.
    * @return                       Returns true if successful.
    */
    bool addData(const QString & text, const SurfaceSet & tSurfaceSet, const AnnotationSet & tAnnotationSet);

    //=========================================================================================================
    /**
    * Adds FreeSurfer brain data.
    *
    * @param[in] pSurface           FreeSurfer surface.
    * @param[in] pAnnotation        FreeSurfer annotation.
    * @return                       Returns true if successful.
    */
    bool addData(const QString & text, const Surface & tSurface, const Annotation & tAnnotation);

    QList<BrainRTDataTreeItem*> addData(const QString & text, const MNESourceEstimate & tSourceEstimate, const MNEForwardSolution & tForwardSolution);

    //=========================================================================================================
    /**
    * Return the brain tree model.
    *
    * @return returns a pointer to the brain tree model BrainTreeModel.
    */
    BrainTreeModel* getBrainTreeModel();    

    void dataChanged(const QModelIndex & topLeft, const QModelIndex & bottomRight, const QVector<int> & roles = QVector<int>());

protected:
    BrainTreeModel*                 m_pBrainTreeModel;
};

} // NAMESPACE

#endif // BRAIN_H