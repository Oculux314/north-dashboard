import { v } from "convex/values";
import { query, mutation, action } from "./_generated/server";
import { api } from "./_generated/api";

// MARK: App API

export const getLogs = query({
  args: {
    start: v.optional(v.number()),
    end: v.optional(v.number()),
  },
  handler: async (ctx, args) => {
    const startTimestamp = args.start ?? 0;
    const endTimestamp = args.end ?? Date.now();

    return await ctx.db.query("logs")
      .withIndex("by_timestamp", (q) =>
        q.gte("timestamp", startTimestamp).lte("timestamp", endTimestamp),
      )
      .order("asc")
      .collect();
  },
});

export const getStartOfLogs = query({
  handler: async (ctx) => {
    const firstLog = await ctx.db
      .query("logs")
      .withIndex("by_timestamp")
      .order("asc")
      .first();
    return firstLog ? firstLog.timestamp : Date.now();
  },
});

// MARK: Arduino API

export const getNow = query({
  handler: async () => {
    return Date.now();
  },
});

export const postLog = mutation({
  args: {
    timestamp: v.number(),
    voltage: v.number(),
    current: v.number(),
  },
  handler: async (ctx, args) => {
    await ctx.db.insert("logs", {
      timestamp: args.timestamp,
      voltage: args.voltage,
      current: args.current,
    });
  },
});
