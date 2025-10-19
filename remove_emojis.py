#!/usr/bin/env python3
"""
Script to remove emojis from markdown files
"""

import os
import re
import glob

def remove_emojis_from_file(filepath):
    """Remove emojis from a markdown file"""
    try:
        with open(filepath, 'r', encoding='utf-8') as f:
            content = f.read()
        
        # Remove emojis (Unicode emoji ranges)
        emoji_pattern = re.compile(
            "["
            "\U0001F600-\U0001F64F"  # emoticons
            "\U0001F300-\U0001F5FF"  # symbols & pictographs
            "\U0001F680-\U0001F6FF"  # transport & map symbols
            "\U0001F1E0-\U0001F1FF"  # flags (iOS)
            "\U00002702-\U000027B0"  # dingbats
            "\U000024C2-\U0001F251"  # enclosed characters
            "\U0001F900-\U0001F9FF"  # supplemental symbols
            "\U0001FA70-\U0001FAFF"  # symbols and pictographs extended-A
            "\U00002600-\U000026FF"  # miscellaneous symbols
            "\U0001F000-\U0001F02F"  # mahjong tiles
            "\U0001F0A0-\U0001F0FF"  # playing cards
            "\U0001F200-\U0001F2FF"  # enclosed CJK letters and months
            "\U0001F300-\U0001F5FF"  # miscellaneous symbols and pictographs
            "\U0001F600-\U0001F64F"  # emoticons
            "\U0001F680-\U0001F6FF"  # transport and map symbols
            "\U0001F700-\U0001F77F"  # alchemical symbols
            "\U0001F780-\U0001F7FF"  # geometric shapes extended
            "\U0001F800-\U0001F8FF"  # supplemental arrows-C
            "\U0001F900-\U0001F9FF"  # supplemental symbols and pictographs
            "\U0001FA00-\U0001FA6F"  # chess symbols
            "\U0001FA70-\U0001FAFF"  # symbols and pictographs extended-A
            "\U00002600-\U000026FF"  # miscellaneous symbols
            "\U00002700-\U000027BF"  # dingbats
            "\U0000FE00-\U0000FE0F"  # variation selectors
            "\U0001F170-\U0001F251"  # enclosed characters
            "]+", 
            flags=re.UNICODE
        )
        
        # Remove emojis
        cleaned_content = emoji_pattern.sub('', content)
        
        # Clean up multiple spaces and empty lines
        cleaned_content = re.sub(r'\n\s*\n\s*\n', '\n\n', cleaned_content)
        cleaned_content = re.sub(r'  +', ' ', cleaned_content)
        
        # Write back the cleaned content
        with open(filepath, 'w', encoding='utf-8') as f:
            f.write(cleaned_content)
        
        print(f"Cleaned: {filepath}")
        return True
        
    except Exception as e:
        print(f"Error processing {filepath}: {e}")
        return False

def main():
    """Main function to process all markdown files"""
    # Get all markdown files
    md_files = []
    
    # Root directory
    md_files.extend(glob.glob("*.md"))
    
    # docs directory
    md_files.extend(glob.glob("docs/*.md"))
    md_files.extend(glob.glob("docs/**/*.md", recursive=True))
    
    # security directory
    md_files.extend(glob.glob("security/**/*.md", recursive=True))
    
    # Remove duplicates
    md_files = list(set(md_files))
    
    print(f"Found {len(md_files)} markdown files")
    
    success_count = 0
    for filepath in md_files:
        if os.path.exists(filepath):
            if remove_emojis_from_file(filepath):
                success_count += 1
    
    print(f"Successfully cleaned {success_count} files")

if __name__ == "__main__":
    main()
