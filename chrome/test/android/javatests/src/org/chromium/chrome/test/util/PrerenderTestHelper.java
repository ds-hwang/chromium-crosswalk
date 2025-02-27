// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.util;

import junit.framework.Assert;

import org.chromium.base.ThreadUtils;
import org.chromium.chrome.browser.TabLoadStatus;
import org.chromium.chrome.browser.prerender.ExternalPrerenderHandler;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeTabbedActivityTestBase;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;

import java.util.concurrent.Callable;

/**
 * Utility class for common methods to test prerendering.
 */
public class PrerenderTestHelper {
    private static final int UI_DELAY_MS = 100;
    private static final int WAIT_FOR_RESPONSE_MS = 10000;
    private static final int SHORT_TIMEOUT_MS = 200;

    private static boolean hasTabPrerenderedUrl(final Tab tab, final String url) {
        return ThreadUtils.runOnUiThreadBlockingNoException(new Callable<Boolean>() {
            @Override
            public Boolean call() {
                return tab.hasPrerenderedUrl(url);
            }
        });
    }

    /**
     * Waits for the specified URL to be prerendered.
     * Returns true if it succeeded, false if it was not prerendered before the timeout.
     * shortTimeout should be set to true when expecting this function to return false, as to
     * make the tests run faster.
     */
    public static boolean waitForPrerenderUrl(final Tab tab, final String url,
            boolean shortTimeout) throws InterruptedException {
        try {
            CriteriaHelper.pollForCriteria(new Criteria() {
                @Override
                public boolean isSatisfied() {
                    return hasTabPrerenderedUrl(tab, url);
                }
            }, shortTimeout ? SHORT_TIMEOUT_MS : WAIT_FOR_RESPONSE_MS, UI_DELAY_MS);
        } catch (AssertionError e) {
            // TODO(tedchoc): This is horrible and should never timeout to determine success.
        }

        return hasTabPrerenderedUrl(tab, url);
    }

    /**
     * Clears the omnibox.
     *
     * @param testBase ChromeTabbedActivityTestBase instance.
     */
    public static void clearOmnibox(ChromeTabbedActivityTestBase testBase)
            throws InterruptedException {
        testBase.typeInOmnibox("", false);
    }

    /**
     * Clears the omnibox and types in the url character-by-character.
     *
     * @param url url to type into the omnibox.
     * @param testBase ChromeTabbedActivityTestBase instance.
     */
    public static void clearOmniboxAndTypeUrl(String url, ChromeTabbedActivityTestBase testBase)
            throws InterruptedException {
        clearOmnibox(testBase);
        testBase.typeInOmnibox(url, true);
    }

    /**
     * Prerenders a url.
     *
     * @param testUrl Url to prerender
     * @param tab The tab to add the prerender to.
     */
    public static ExternalPrerenderHandler prerenderUrl(final String testUrl, Tab tab)
            throws InterruptedException {
        final Tab currentTab = tab;

        ExternalPrerenderHandler prerenderHandler = ThreadUtils.runOnUiThreadBlockingNoException(
                new Callable<ExternalPrerenderHandler>() {
                    @Override
                    public ExternalPrerenderHandler call() throws Exception {
                        ExternalPrerenderHandler prerenderHandler = new ExternalPrerenderHandler();
                        boolean didPrerender = prerenderHandler.addPrerender(
                                currentTab.getProfile(), currentTab.getWebContents(), testUrl, null,
                                currentTab.getContentViewCore().getRenderCoordinates()
                                        .getContentWidthPixInt(),
                                currentTab.getContentViewCore().getRenderCoordinates()
                                        .getContentHeightPixInt(),
                                false);
                        Assert.assertTrue("Failed to prerender test url: " + testUrl, didPrerender);
                        return prerenderHandler;
                    }
                });

        Assert.assertTrue("URL was not prerendered.",
                PrerenderTestHelper.waitForPrerenderUrl(currentTab, testUrl, false));

        return prerenderHandler;
    }

    /**
     * Checks if the load url result is prerendered.
     *
     * @param result Result from a page load.
     * @return Whether the result param indicates a prerendered url.
     */
    public static boolean isLoadUrlResultPrerendered(int result) {
        return result == TabLoadStatus.FULL_PRERENDERED_PAGE_LOAD
                || result == TabLoadStatus.PARTIAL_PRERENDERED_PAGE_LOAD;
    }
}
