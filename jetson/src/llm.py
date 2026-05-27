import asyncio
from dataclasses import dataclass
from collections.abc import Callable
from pydantic import BaseModel, Field
from typing import Literal
from google import genai
from google.genai import types

from dependencies import secrets

def dummy1(param1, param2):
    return ""

def dummy2(param1, param2):
    return ""

def dummy3(param1, param2):
    return ""

MODEL = "gemini-3.1-flash-lite-preview"
AGENT_CONFIG = types.GenerateContentConfig(
    system_instruction = """system instruction not set""",
    tools = [dummy1, dummy2, dummy3],
    temperature = 0.7,
    max_output_tokens = 500
)

COMMAND_CONFIG = types.GenerateContentConfig(
    system_instruction = """You are a command parser for a robot. Convert the user's natural language input into a JSON array representing a command chain.

Output ONLY valid JSON, no explanation, no markdown, no backticks.

Rules:
- Outer array is sequential (connected by "then")
- Inner arrays are parallel groups (connected by "and")  
- Each object must have an "action" field from: move, rotate, attack, stop, interact, follow
- Include only parameters that are explicitly stated or clearly implied
- Valid parameter keys: direction, power (0-100), duration (ms), degrees, rotations
- Valid directions for move: forward, backward
- Valid directions for rotate: clockwise, counterclockwise
- Do not include defaults, the builder will fill those in

Example input: "go forward then turn 45 degrees clockwise and shoot"
Example output: [[{"action":"move","direction":"forward"}],[{"action":"rotate","degrees":45,"direction":"clockwise"},{"action":"attack"}]]""",
    temperature=0.1,
    max_output_tokens=100
)

class LLMAgent:
    """Wrapper class for llm agent api"""
    _instance = None
    _initialized = False

    def __new__(cls):
        if cls._instance is None:
            cls._instance = super(LLMAgent, cls).__new__(cls)
        return cls._instance
    
    def __init__(self):
        if self._initialized: return
        
        api_key = secrets("api_key", "llm")
        if api_key == "":
            raise RuntimeError("LLMAgent must be initialized with an api_key first")

        self._history: dict[str, list] = {}

        self._client = genai.Client(api_key=api_key)
        self._chat = self._client.chats.create(model=MODEL, config=AGENT_CONFIG)
        self._command_parser_chat = self._client.chats.create(model=MODEL, config=COMMAND_CONFIG)

        self._initialized = True

    async def process_text(self, text, speaker):
        """Determine whether the prompt is a command, needs a response, or both"""
        response = await asyncio.to_thread(self._chat.send_message, f"Speaker: {speaker}, Message: {text}")

        if response is None:
            raise RuntimeError("Error receiving response from LLM")

        if response.candidates and response.candidates[0].content and response.candidates[0].content.parts and response.candidates[0].content.parts[0].function_call:
            fn = response.candidates[0].content.parts[0].function_call
            print(f"Executing function call: {fn.name} with {fn.args}")  
            #TODO: Add function call     

        return response.text

    async def process_command(self, text):
        response = await asyncio.to_thread(self._command_parser_chat.send_message, text)

        if response is None:
            raise RuntimeError("Error receiving response from LLM")

        return response.text