#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# TBD

class ESBFrameFactory:
    def __init__(self, payload):
        self.payload = payload

class ESBTransactionFactory:
    """
    Put a payload into ESB packets for transmission
    """
    def __init__(self, src, dst, **params):
        self.src = src
        self.dst = dst
