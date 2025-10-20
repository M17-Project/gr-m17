#!/usr/bin/env python3
"""
update_pydoc.py - Extract docstrings from doxygen documentation for Python bindings

This script extracts docstrings from doxygen-generated documentation
and creates a JSON file that can be used by Python bindings.
"""

import json
import os
import sys
import xml.etree.ElementTree as ET
from pathlib import Path

def extract_docstrings_from_xml(xml_file, output_file):
    """
    Extract docstrings from doxygen XML output and create JSON for Python bindings.
    """
    try:
        # Parse the doxygen XML file
        tree = ET.parse(xml_file)
        root = tree.getroot()
        
        docstrings = {}
        
        # Extract function documentation
        for member in root.findall('.//memberdef[@kind="function"]'):
            name_elem = member.find('name')
            if name_elem is not None:
                func_name = name_elem.text
                
                # Get brief description
                brief_elem = member.find('briefdescription')
                brief = ""
                if brief_elem is not None:
                    brief = "".join(brief_elem.itertext()).strip()
                
                # Get detailed description
                detailed_elem = member.find('detaileddescription')
                detailed = ""
                if detailed_elem is not None:
                    detailed = "".join(detailed_elem.itertext()).strip()
                
                # Combine brief and detailed descriptions
                full_desc = brief
                if detailed and detailed != brief:
                    full_desc += "\n\n" + detailed
                
                if full_desc:
                    docstrings[func_name] = full_desc
        
        # Write the extracted docstrings to JSON
        with open(output_file, 'w') as f:
            json.dump(docstrings, f, indent=2)
        
        print(f"Extracted {len(docstrings)} docstrings to {output_file}")
        return True
        
    except Exception as e:
        print(f"Error extracting docstrings: {e}")
        return False

def main():
    """
    Main function to extract docstrings from doxygen documentation.
    """
    # Default paths
    xml_dir = Path(__file__).parent / "html" / "xml"
    output_file = Path(__file__).parent.parent.parent / "build" / "python" / "m17" / "bindings" / "extracted_docstrings.json"
    
    # Create output directory if it doesn't exist
    output_file.parent.mkdir(parents=True, exist_ok=True)
    
    # Look for doxygen XML files
    xml_files = list(xml_dir.glob("*.xml")) if xml_dir.exists() else []
    
    if not xml_files:
        print("No doxygen XML files found, creating empty docstrings file")
        # Create empty docstrings file
        with open(output_file, 'w') as f:
            json.dump({}, f, indent=2)
        return 0
    
    # Extract docstrings from the first XML file (usually the main one)
    main_xml = xml_files[0]
    if extract_docstrings_from_xml(main_xml, output_file):
        return 0
    else:
        return 1

if __name__ == "__main__":
    sys.exit(main())
