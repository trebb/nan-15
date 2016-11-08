# +---+---+---+---+
# | 7 | 8 | 9 | T |
# +---+---+---+---+
# | 4 | 5 | 6 | S |
# +---+---+---+---+
# | 1 | 2 | 3 | R |
# +---+---+---+---+
# +---------------+
# | 0 |   D   | O |
# +---+-------+---+

sed -e '/^  /s/[^#]/ /g' <<EOF
78=
           
   # # 9 T 
    ##     
   4 # 6 S 
           
   1 2 3 R 
           
79=
           
   # 8 # T 
    #  #   
   4 ### S 
           
   1 2 3 R 
           
7T=
           
   # 8 9 # 
    #   #  
   4 ### S 
           
   1 2 3 R 
           
74=
           
   # 8 9 T 
    #      
   ### 6 S 
           
   1 2 3 R 
           
7S=
           
   # 8 9 T 
    #      
   4 ##### 
           
   1 2 3 R 
           
71=
           
   # 8 9 T 
    #      
   4 # 6 S 
    #      
   # 2 3 R 
           
72=
           
   # 8 9 T 
    #      
   4 # 6 S 
     #     
   1 # 3 R 
           
73=
           
   # 8 9 T 
    #      
   4 ### S 
       #   
   1 2 # R 
           
7R=
           
   # 8 9 T 
    #      
   4 ### S 
        #  
   1 2 3 # 
           
  
89=
           
   7 # # T 
     # #   
   4 ### S 
           
   1 2 3 R 
           
8T=
           
   7 # 9 # 
     #  #  
   4 ### S 
           
   1 2 3 R 
           
84=
           
   7 # 9 T 
     #     
   ### 6 S 
           
   1 2 3 R 
           
8S=
           
   7 # 9 T 
     #     
   4 ##### 
           
   1 2 3 R 
           
81=
           
   7 # 9 T 
     #     
   4 # 6 S 
    #      
   # 2 3 R 
           
82=
           
   7 # 9 T 
     #     
   4 # 6 S 
     #     
   1 # 3 R 
           
83=
           
   7 # 9 T 
     #     
   4 ### S 
       #   
   1 2 # R 
           
8R=
           
   7 # 9 T 
     #     
   4 ### S 
        #  
   1 2 3 # 
           
  
9T=
           
   7 8 # # 
       ##  
   4 5 # S 
           
   1 2 3 R 
           
94=
           
   7 8 # T 
       #   
   ##### S 
           
   1 2 3 R 
           
9S=
           
   7 8 # T 
       #   
   4 5 ### 
           
   1 2 3 R 
           
91=
           
   7 8 # T 
       #   
   4 ### S 
    #      
   # 2 3 R 
           
92=
           
   7 8 # T 
       #   
   4 ### S 
     #     
   1 # 3 R 
           
93=
           
   7 8 # T 
       #   
   4 5 # S 
       #   
   1 2 # R 
           
9R=
           
   7 8 # T 
       #   
   4 5 # S 
        #  
   1 2 3 # 
           
  
T4=
           
   7 8 9 # 
        #  
   ##### S 
           
   1 2 3 R 
           
TS=
           
   7 8 9 # 
        #  
   4 5 ### 
           
   1 2 3 R 
           
T1=
           
   7 8 9 # 
        #  
   4 ### S 
    #      
   # 2 3 R 
           
T2=
           
   7 8 9 # 
        #  
   4 ### S 
     #     
   1 # 3 R 
           
T3=
           
   7 8 9 # 
        #  
   4 5 # S 
       #   
   1 2 # R 
           
TR=
           
   7 8 9 # 
        #  
   4 5 # S 
        #  
   1 2 3 # 
           
  
4S=
           
   7 8 9 T 
           
   ####### 
           
   1 2 3 R 
           
41=
           
   7 8 9 T 
           
   ### 6 S 
    #      
   # 2 3 R 
           
42=
           
   7 8 9 T 
           
   ### 6 S 
     #     
   1 # 3 R 
           
43=
           
   7 8 9 T 
           
   ##### S 
       #   
   1 2 # R 
           
4R=
           
   7 8 9 T 
           
   ##### S 
        #  
   1 2 3 # 
           
  
S1=
           
   7 8 9 T 
           
   4 ##### 
    #      
   # 2 3 R 
           
S2=
           
   7 8 9 T 
           
   4 ##### 
     #     
   1 # 3 R 
           
S3=
           
   7 8 9 T 
           
   4 5 ### 
       #   
   1 2 # R 
           
SR=
           
   7 8 9 T 
           
   4 5 ### 
        #  
   1 2 3 # 
           
  
12=
           
   7 8 9 T 
           
   4 # 6 S 
    ##     
   # # 3 R 
           
13=
           
   7 8 9 T 
           
   4 ### S 
    #  #   
   # 2 # R 
           
1R=
           
   7 8 9 T 
           
   4 ### S 
    #   #  
   # 2 3 # 
           
  
23=
           
   7 8 9 T 
           
   4 ### S 
     # #   
   1 # # R 
           
2R=
           
   7 8 9 T 
           
   4 ### S 
     #  #  
   1 # 3 # 
           
  
3R=
           
   7 8 9 T 
           
   4 5 # S 
       ##  
   1 2 # # 
           
EOF
