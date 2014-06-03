///  @file    TestParameter.cs
///  @brief   Parameter unit tests
///  @author  Frank Bergmann (Csharp conversion)
///  @author  Akiya Jouraku (Csharp conversion)
///  @author  Ben Bornstein 
/// 
/// 
///  ====== WARNING ===== WARNING ===== WARNING ===== WARNING ===== WARNING ======
/// 
///  DO NOT EDIT THIS FILE.
/// 
///  This file was generated automatically by converting the file located at
///  src/sbml/test/TestParameter.c
///  using the conversion program dev/utilities/translateTests/translateTests.pl.
///  Any changes made here will be lost the next time the file is regenerated.
/// 
///  -----------------------------------------------------------------------------
///  This file is part of libSBML.  Please visit http://sbml.org for more
///  information about SBML, and the latest version of libSBML.
/// 
///  Copyright 2005-2010 California Institute of Technology.
///  Copyright 2002-2005 California Institute of Technology and
///                      Japan Science and Technology Corporation.
///  
///  This library is free software; you can redistribute it and/or modify it
///  under the terms of the GNU Lesser General Public License as published by
///  the Free Software Foundation.  A copy of the license agreement is provided
///  in the file named "LICENSE.txt" included with this software distribution
///  and also available online as http://sbml.org/software/libsbml/license.html
///  -----------------------------------------------------------------------------


namespace LibSBMLCSTest.sbml {

  using libsbmlcs;

  using System;

  using System.IO;

  public class TestParameter {
    public class AssertionError : System.Exception 
    {
      public AssertionError() : base()
      {
        
      }
    }


    static void assertTrue(bool condition)
    {
      if (condition == true)
      {
        return;
      }
      throw new AssertionError();
    }

    static void assertEquals(object a, object b)
    {
      if ( (a == null) && (b == null) )
      {
        return;
      }
      else if ( (a == null) || (b == null) )
      {
        throw new AssertionError();
      }
      else if (a.Equals(b))
      {
        return;
      }
  
      throw new AssertionError();
    }

    static void assertNotEquals(object a, object b)
    {
      if ( (a == null) && (b == null) )
      {
        throw new AssertionError();
      }
      else if ( (a == null) || (b == null) )
      {
        return;
      }
      else if (a.Equals(b))
      {
        throw new AssertionError();
      }
    }

    static void assertEquals(bool a, bool b)
    {
      if ( a == b )
      {
        return;
      }
      throw new AssertionError();
    }

    static void assertNotEquals(bool a, bool b)
    {
      if ( a != b )
      {
        return;
      }
      throw new AssertionError();
    }

    static void assertEquals(int a, int b)
    {
      if ( a == b )
      {
        return;
      }
      throw new AssertionError();
    }

    static void assertNotEquals(int a, int b)
    {
      if ( a != b )
      {
        return;
      }
      throw new AssertionError();
    }

    private Parameter P;

    public void setUp()
    {
      P = new  Parameter(2,4);
      if (P == null);
      {
      }
    }

    public void tearDown()
    {
      P = null;
    }

    public void test_Parameter_create()
    {
      assertTrue( P.getTypeCode() == libsbml.SBML_PARAMETER );
      assertTrue( P.getMetaId() == "" );
      assertTrue( P.getNotes() == null );
      assertTrue( P.getAnnotation() == null );
      assertTrue( P.getId() == "" );
      assertTrue( P.getName() == "" );
      assertTrue( P.getUnits() == "" );
      assertTrue( P.getConstant() == true );
      assertEquals( false, P.isSetId() );
      assertEquals( false, P.isSetName() );
      assertEquals( false, P.isSetValue() );
      assertEquals( false, P.isSetUnits() );
      assertEquals( true, P.isSetConstant() );
    }

    public void test_Parameter_createWithNS()
    {
      XMLNamespaces xmlns = new  XMLNamespaces();
      xmlns.add( "http://www.sbml.org", "testsbml");
      SBMLNamespaces sbmlns = new  SBMLNamespaces(2,1);
      sbmlns.addNamespaces(xmlns);
      Parameter object1 = new  Parameter(sbmlns);
      assertTrue( object1.getTypeCode() == libsbml.SBML_PARAMETER );
      assertTrue( object1.getMetaId() == "" );
      assertTrue( object1.getNotes() == null );
      assertTrue( object1.getAnnotation() == null );
      assertTrue( object1.getLevel() == 2 );
      assertTrue( object1.getVersion() == 1 );
      assertTrue( object1.getNamespaces() != null );
      assertTrue( object1.getNamespaces().getLength() == 2 );
      object1 = null;
    }

    public void test_Parameter_free_NULL()
    {
    }

    public void test_Parameter_setId()
    {
      string id =  "Km1";
      P.setId(id);
      assertTrue(( id == P.getId() ));
      assertEquals( true, P.isSetId() );
      if (P.getId() == id);
      {
      }
      P.setId(P.getId());
      assertTrue(( id == P.getId() ));
      P.setId("");
      assertEquals( false, P.isSetId() );
      if (P.getId() != null);
      {
      }
    }

    public void test_Parameter_setName()
    {
      string name =  "Forward_Michaelis_Menten_Constant";
      P.setName(name);
      assertTrue(( name == P.getName() ));
      assertEquals( true, P.isSetName() );
      if (P.getName() == name);
      {
      }
      P.setName(P.getName());
      assertTrue(( name == P.getName() ));
      P.setName("");
      assertEquals( false, P.isSetName() );
      if (P.getName() != null);
      {
      }
    }

    public void test_Parameter_setUnits()
    {
      string units =  "second";
      P.setUnits(units);
      assertTrue(( units == P.getUnits() ));
      assertEquals( true, P.isSetUnits() );
      if (P.getUnits() == units);
      {
      }
      P.setUnits(P.getUnits());
      assertTrue(( units == P.getUnits() ));
      P.setUnits("");
      assertEquals( false, P.isSetUnits() );
      if (P.getUnits() != null);
      {
      }
    }

  }
}

