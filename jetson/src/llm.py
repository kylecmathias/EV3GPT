import asyncio
from dataclasses import dataclass
from collections.abc import Callable
from pydantic import BaseModel, Field
from typing import Literal
from google import genai
from google.genai import types

def dummy1(param1, param2):
    return ""

def dummy2(param1, param2):
    return ""

def dummy3(param1, param2):
    return ""

AGENT_CONFIG = types.GenerateContentConfig(
    system_instruction = """system instruction not set""",
    tools = [dummy1, dummy2, dummy3],
    temperature = 0.7,
    max_output_tokens = 500
)

class LLMAgent:
    """Wrapper class for llm agent api"""
    _instance = None
    _initialized = False

    def __new__(cls):
        if cls._instance is None:
            cls._instance = super(LLMAgent, cls).__new__(cls)
        return cls._instance
    
    def __init__(self, api_key):
        if self._initialized: return

        self._history: dict[str, list] = {}

        self._client = genai.Client(api_key=api_key)
        self._model_id = "gemini-3.1-flash-lite-preview"
        self._config = AGENT_CONFIG
        self._chat = self._client.chats.create(model=self._model_id, config=self._config)

        self._initalized = True

    async def process_text(self, text, speaker):
        """Determine whether the prompt is a command, needs a response, or both"""
        response = await asyncio.to_thread(self._chat.send_message, f"Speaker: {speaker}, Message: {text}")

        if response is None:
            raise RuntimeError("Error receiving response from LLM")

        if response.candidates and response.candidates[0].content and response.candidates[0].content.parts and response.candidates[0].content.parts[0].function_call:
            fn = response.candidates[0].content.parts[0].function_call
            print(f"Executing function call: {fn.name} with {fn.args}")

            

        return response.text