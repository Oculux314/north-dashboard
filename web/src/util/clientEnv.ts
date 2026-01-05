// This file reads environment variables from an .env file and validates them with zod
// It exports the validated environment variables as an object to be used throughout the client

import { z } from "zod";

// .env schema
const clientEnvSchema = z.object({
  VITE_CONVEX_URL: z.url("VITE_CONVEX_URL must be a valid URL"),
  NODE_ENV: z
    .enum(["development", "production"], {
      message: "NODE_ENV must be either 'development' or 'production'",
    })
    .default("development"),
});

// Read .env, validate and export it
export const clientEnv = clientEnvSchema.parse(import.meta.env);
export type ClientEnv = z.infer<typeof clientEnvSchema>;
