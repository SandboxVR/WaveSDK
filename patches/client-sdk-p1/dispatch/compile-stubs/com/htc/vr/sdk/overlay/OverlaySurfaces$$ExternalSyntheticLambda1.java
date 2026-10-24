package com.htc.vr.sdk.overlay;
final class OverlaySurfaces$$ExternalSyntheticLambda1 implements Runnable {
    private final OverlaySurfaces owner;
    OverlaySurfaces$$ExternalSyntheticLambda1(OverlaySurfaces owner) { this.owner = owner; }
    public void run() { owner.patchQuitOnThread(); }
}
