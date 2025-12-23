 curl --location 'https://dashscope.aliyuncs.com/api/v1/services/aigc/multimodal-generation/generation' \
--header 'Content-Type: application/json' \                                     
--header "Authorization: Bearer $DASHSCOPE_API_KEY" \                 
--data '{                                                         
    "model": "qwen-image-plus",
    "input": {                                                              
        "messages": [                                                       
            {                  
                "role": "user", 
                "content": [    
                    {      
                        "text": "一副典雅庄重的对联悬挂于厅堂之中，房间是个安静古典的中式布置，桌子上放着一些青花瓷，对联上左书“义本生知人机同道善思新”，右书“通云赋智乾坤启数高志远”， 横批“智启通义”，字体飘逸，在中间挂着一幅 中国风的画作，内容是岳阳楼。"
                    }
                ]
            }
        ]
    },
    "parameters": {
        "negative_prompt": "",
        "prompt_extend": true,
        "watermark": false,
        "size": "1328*1328"
    }
}'
