/**
 * @file:   RenderSBMLDocumentPlugin.cpp
 * @brief:  Implementation of the RenderSBMLDocumentPlugin class
 * @author: SBMLTeam
 *
 * <!--------------------------------------------------------------------------
 * This file is part of libSBML.  Please visit http://sbml.org for more
 * information about SBML, and the latest version of libSBML.
 *
 * Copyright (C) 2009-2013 jointly by the following organizations:
 *     1. California Institute of Technology, Pasadena, CA, USA
 *     2. EMBL European Bioinformatics Institute (EMBL-EBI), Hinxton, UK
 *
 * Copyright (C) 2006-2008 by the California Institute of Technology,
 *     Pasadena, CA, USA 
 *
 * Copyright (C) 2002-2005 jointly by the following organizations:
 *     1. California Institute of Technology, Pasadena, CA, USA
 *     2. Japan Science and Technology Agency, Japan
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.  A copy of the license agreement is provided
 * in the file named "LICENSE.txt" included with this software distribution
 * and also available online as http://sbml.org/software/libsbml/license.html
 * ------------------------------------------------------------------------ -->
 */
#include <sbml/packages/render/extension/RenderSBMLDocumentPlugin.h>


#ifdef __cplusplus

/** @cond doxygenLibsbmlInternal */

using namespace std;

/** @endcond doxygenLibsbmlInternal */

LIBSBML_CPP_NAMESPACE_BEGIN

RenderSBMLDocumentPlugin::RenderSBMLDocumentPlugin (const string &uri, 
                              const string &prefix, RenderPkgNamespaces *renderns)
  : SBMLDocumentPlugin(uri,prefix, renderns)
{
}


RenderSBMLDocumentPlugin::RenderSBMLDocumentPlugin(const RenderSBMLDocumentPlugin& orig)
  : SBMLDocumentPlugin(orig)
{
}


RenderSBMLDocumentPlugin& 
RenderSBMLDocumentPlugin::operator=(const RenderSBMLDocumentPlugin& orig)
{
  if(&orig!=this)
  {
    SBMLDocumentPlugin::operator =(orig);
  }    
  return *this;
}


/*
 * Creates and returns a deep copy of this RenderSBMLDocumentPlugin object.
 */
RenderSBMLDocumentPlugin* 
RenderSBMLDocumentPlugin::clone () const
{
  return new RenderSBMLDocumentPlugin(*this);  
}

/*
 * Destroy this object.
 */
RenderSBMLDocumentPlugin::~RenderSBMLDocumentPlugin () 
{
}


/** @cond doxygenLibsbmlInternal */

void 
RenderSBMLDocumentPlugin::readAttributes (const XMLAttributes& attributes,
                            const ExpectedAttributes& expectedAttributes)
{
  // for now don't read the required flag for L2 models 
  if (getSBMLDocument() != NULL && getSBMLDocument()->getLevel() < 3) return;
  
  unsigned int numErrs = getErrorLog()->getNumErrors();
  XMLTriple tripleRequired("required", mURI, getPrefix());
  bool assigned = attributes.readInto(tripleRequired, mRequired);
  if (assigned == false)
  {
    if (getErrorLog()->getNumErrors() == numErrs + 1 && 
        getErrorLog()->contains(XMLAttributeTypeMismatch))
    {
      getErrorLog()->remove(XMLAttributeTypeMismatch);
      getErrorLog()->logPackageError("render", UnknownPackageAttribute,
        getPackageVersion(), getLevel(), getVersion(), "required attribute for render must be a boolean", getLine(), getColumn());
    }
    else
    {
      getErrorLog()->logPackageError("render", UnknownPackageAttribute,
        getPackageVersion(), getLevel(), getVersion(), "Render Namespace given, but the required attribute is not present", getLine(), getColumn(), LIBSBML_SEV_WARNING);
    }
  }
  else
  {
    mIsSetRequired = true;
	/* LOG ERROR RELATING TO EXPECTED VALUE */
    //if (mRequired == true)
    //{
    //  getErrorLog()->logPackageError("render", ERROR,
    //    getPackageVersion(), getLevel(), getVersion());
    //}
  }
}

/** @endcond doxygenLibsbmlInternal*/


bool
RenderSBMLDocumentPlugin::isCompFlatteningImplemented() const
{
  return true;//false;
}

#if (0)

unsigned int 
RenderSBMLDocumentPlugin::checkConsistency()
{
  unsigned int nerrors = 0;
  unsigned int total_errors = 0;

  
  SBMLDocument* doc = static_cast<SBMLDocument *>(this->getParentSBMLObject());
  SBMLErrorLog *log = doc->getErrorLog();

  unsigned char applicableValidators = doc->getApplicableValidators();

  /* determine which validators to run */
  bool id    = ((applicableValidators & 0x01) == 0x01);
  bool sbml  = ((applicableValidators & 0x02) == 0x02);
  /* LIST OTHERS HERE */

  RenderIdentifierConsistencyValidator id_validator;
  RenderConsistencyValidator validator;
  /* LIST OTHERS HERE */

  if (id)
  {
    id_validator.init();
    nerrors = id_validator.validate(*doc);
    total_errors += nerrors;
    if (nerrors > 0) 
    {
      log->add(id_validator.getFailures() );
      /* only want to bail if errors not warnings */
      if (log->getNumFailsWithSeverity(LIBSBML_SEV_ERROR) > 0)
      {
        return total_errors;
      }
    }
  }

  if (sbml)
  {
    validator.init();
    nerrors = validator.validate(*doc);
    total_errors += nerrors;
    if (nerrors > 0) 
    {
      log->add(validator.getFailures() );
	  // DO NOT NEED THIS IN LAST CALL
      // /* only want to bail if errors not warnings */
      // if (log->getNumFailsWithSeverity(LIBSBML_SEV_ERROR) > 0)
      // {
      //   return total_errors;
      // }
    }
  }

  /* ADD OTHERS HERE */


  return total_errors;  
}
#endif

LIBSBML_CPP_NAMESPACE_END

#endif  /* __cplusplus */
