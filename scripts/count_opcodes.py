import re
import os

def count_opcodes():
    cpu_cpp_path = os.path.join('src', 'core', 'cpu.cpp')
    
    if not os.path.exists(cpu_cpp_path):
        print(f"Error: Could not find {cpu_cpp_path}")
        return

    try:
        with open(cpu_cpp_path, 'r') as f:
            content = f.read()
            
        # Find the init_instructions function block to ensure we only count there
        # This is a simple extraction, looking for the function body
        start_marker = "void CPU::init_instructions() {"
        end_marker = "}"
        
        start_idx = content.find(start_marker)
        if start_idx == -1:
            print("Error: Could not find init_instructions() in cpu.cpp")
            return
            
        # Simplistic finding of the end of the function (assuming consistent indentation/structure or just counting braces)
        # But scanning line by line inside the function is safer regarding the pattern.
        
        # We will just scan the whole file for the specific assignment pattern 
        # instructions[0xXX] = ...
        # This is strictly for the assignments in init_instructions.
        
        pattern = re.compile(r'instructions\[0x[0-9A-Fa-f]{2}\]\s*=\s*\{')
        matches = pattern.findall(content)
        
        count = len(matches)
        total = 256
        percentage = (count / total) * 100
        
        print("-" * 40)
        print(f"GameByte Opcode Implementation Status")
        print("-" * 40)
        print(f"Implemented: {count}/{total} ({percentage:.1f}%)")
        print("-" * 40)

    except Exception as e:
        print(f"An error occurred: {e}")

if __name__ == "__main__":
    count_opcodes()
