/*
 * pydoc_macros.h
 * 
 * GNU Radio M17 Module - Python Documentation Macros
 * 
 * This file contains macros for Python documentation generation.
 * It's used by the Python bindings to generate proper documentation.
 */

#ifndef PYDOC_MACROS_H
#define PYDOC_MACROS_H

// Standard Python documentation macros
#define D(...) __VA_ARGS__

// Function documentation macros
#define DOCSTRING(...) __VA_ARGS__

// Class documentation macros  
#define CLASS_DOCSTRING(...) __VA_ARGS__

// Module documentation macros
#define MODULE_DOCSTRING(...) __VA_ARGS__

#endif // PYDOC_MACROS_H
